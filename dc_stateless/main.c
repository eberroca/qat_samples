#include "main.h"

#define INPUT_FILE "./el_quijote.txt"

int main(void)
{
	CpaInstanceHandle            dcInstHandles[MAX_INSTANCES];
	Cpa16U                       numInstances                   = 0;
	CpaStatus                    status                         = CPA_STATUS_SUCCESS;
	Cpa16U                       numBuffers                     = 0;
	CpaDcInstanceCapabilities    capabilities                   = {0};
	Cpa32U                       buffMetaSize                   = 0;
	CpaBufferList              **buffList                       = NULL;
	CpaDcSessionSetupData        sessionData                    = {0};
	Cpa32U                       sessionSize                    = 0;
	Cpa32U                       ctxSize                        = 0;
	pthread_t                    pollingThread;
	CpaDcSessionHandle           sessionHdl                     = NULL;
	CpaStatus                    sessionStatus                  = CPA_STATUS_SUCCESS;


	/* Initiating memory driver */
	status = memInit();
	if (CPA_STATUS_SUCCESS != status) {
		printf("failed to initialize memory driver\n");
		return (int)status;
	}

	/* starting the user process */
	status = icp_sal_userStart("SSL");
	if (CPA_STATUS_SUCCESS != status) {
		printf("failed to start user process SSL\n");
		return (int)status;
	}

	/* getting the number of instances */
	status = cpaDcGetNumInstances(&numInstances);
	if (CPA_STATUS_SUCCESS != status) {
		printf("unable to get number of DC instances\n");
		return (int)status;
	}
	if (0 == numInstances) {
		printf("no DC instances found!\n");
		return (int)CPA_STATUS_FAIL;
	}
	printf("number of DC Instances=%d\n", (int)numInstances);

	/* getting the DC instances */
	status = cpaDcGetInstances(numInstances, dcInstHandles);
	if (CPA_STATUS_SUCCESS != status) {
		printf("unable to get DC instances\n");
		return (int)status;
	}

	/********************************************
	 * we use just one instance in this example (dcInstHandles[0])
	 */

	/* query capabilities */
	status = cpaDcQueryCapabilities(dcInstHandles[0], &capabilities);
	if (CPA_STATUS_SUCCESS != status) {
		printf("unable to query instance's capabilities\n");
		return (int)status;
	}
	if (!capabilities.statelessDeflateCompression ||
	    !capabilities.statelessDeflateDecompression ||
	    !capabilities.checksumAdler32 ||
	    !capabilities.dynamicHuffman) {
		printf("error: unsupported functionality\n");
		return (int)CPA_STATUS_FAIL;
	}
	printf("capabilities.dynamicHuffmanBufferReq=%d\n", capabilities.dynamicHuffmanBufferReq);
	if (capabilities.dynamicHuffmanBufferReq) {
		status = cpaDcBufferListGetMetaSize(dcInstHandles[0], 1, &buffMetaSize);
		printf("getting buff meta size status=%d\n", (int)status);
		if (CPA_STATUS_SUCCESS == status) {
			status = cpaDcGetNumIntermediateBuffers(dcInstHandles[0], &numBuffers);
		}
		printf("buffers -> status=%d numBuffers=%d\n", (int)status, (int)numBuffers);
	}

	/* set the memory address translation function for the instance (user space) */
	if (CPA_STATUS_SUCCESS == status) {
		status = cpaDcSetAddressTranslation(dcInstHandles[0], qaeVirtToPhysNUMA);
	}

	/* start DataCompression component */
	if (CPA_STATUS_SUCCESS == status) {
		status = cpaDcStartInstance(dcInstHandles[0], numBuffers, buffList);
		printf("cpaDcStartInstance() called, status=%d\n", (int)status);
	}
	if (CPA_STATUS_SUCCESS == status) {
		/*
		 * start polling thread for polled instance (for user space, it is always polled.
		 * Nevertheless, this can be checked calling cpaDcInstanceGetInfo2() and querying
		 * for "info2.isPolled == CPA_TRUE").
		 * NOTE: How polling is done is implementation-dependent!.
		 */
		if (pthread_create(&pollingThread, NULL, pollingThreadFunc,
				   dcInstHandles[0]) != 0) {
			printf("failed creating the polling thread!\n");
			return (int)CPA_STATUS_FAIL;
		} else {
			pthread_detach(pollingThread);
		}

		/* populating the fields of the session operational data */
		sessionData.compLevel = CPA_DC_L4;       /* Level 4 */
		sessionData.compType  = CPA_DC_DEFLATE;  /* compresion type = deflate */
		sessionData.huffType  = CPA_DC_HT_FULL_DYNAMIC; /* this is for stateless */

		if (capabilities.autoSelectBestHuffmanTree) {
			/* static Huffman encoding over dynamic for better compressibility */
			sessionData.autoSelectBestHuffmanTree = CPA_DC_ASB_STATIC_DYNAMIC;
		} else {
			sessionData.autoSelectBestHuffmanTree = CPA_DC_ASB_DISABLED;
		}
		sessionData.sessDirection = CPA_DC_DIR_COMBINED;
		sessionData.sessState     = CPA_DC_STATELESS;
		sessionData.checksum      = CPA_DC_ADLER32;

		printf("CPA_DC_API_VERSION_NUM_MAJOR=%d CPA_DC_API_VERSION_NUM_MINOR=%d\n",
		       CPA_DC_API_VERSION_NUM_MAJOR, CPA_DC_API_VERSION_NUM_MINOR);

		/* determine size of session context to allocate */
		status = cpaDcGetSessionSize(dcInstHandles[0], &sessionData, &sessionSize,
					     &ctxSize);
		printf("status=%d sessionSize=%d ctxSize=%d\n", (int)status, sessionSize, ctxSize);
	}

	/* allocate session memory */
	if (CPA_STATUS_SUCCESS == status) {
		/* memory allocation in user sapce */
		status = memAllocContig(&sessionHdl, sessionSize, 0, 1); /* node=0, alignment=1 */
	}
	printf("sessionHdl=%p status=%d\n", (void *)sessionHdl, (int)status);

	/* initialize the stateless session */
	if (CPA_STATUS_SUCCESS == status) {
		status = cpaDcInitSession(dcInstHandles[0],
					  sessionHdl,       /* session memory */
					  &sessionData,     /* session setup */
					  NULL,             /* pContexBuffer not required
							       (stateless) */
					  dcCallback);      /* callback function */
	}
	printf("cpaDcInitSession, status=%d\n", (int)status);

	if (CPA_STATUS_SUCCESS == status) {
		/* perform compression operation (just one this time) */
		status = doCompression(INPUT_FILE, dcInstHandles[0], sessionHdl,
				       sessionData.huffType);

		/* tear down the session */
		sessionStatus = cpaDcRemoveSession(dcInstHandles[0], sessionHdl);

		if (CPA_STATUS_SUCCESS == status) {
			status = sessionStatus;
		}
	}

	/* querying the statistics on the instance */

	/*************/
	fflush(stdout);

	/* stopping the polling thread and instance */
	turnPollingOff();
	sleepForMs(10); /* waiting for thread to shut down */

	cpaDcStopInstance(dcInstHandles[0]);

	/* free session context */
	/* (user space) */
	memFreeNUMA((void **)&sessionHdl);
	sessionHdl = NULL;

	if (CPA_STATUS_SUCCESS == status) {
		printf("sample code run successfully\n");
	} else {
		printf("sample code failed with status of %d\n", (int)status);
	}

	/* stopping the user process */
	icp_sal_userStop();

	/* destorying special memory */
	memDestroy();

	return 0;
}
