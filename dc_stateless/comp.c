#include "main.h"


CpaStatus doCompression(const char *file, CpaInstanceHandle dcInstHandle,
			CpaDcSessionHandle sessionHdl, CpaDcHuffType huffType)
{
	int                        ret                = 0;
	CpaDcOpData                opData             = {};        /* operational data */
	CpaStatus                  status;
	CpaDcInstanceCapabilities  cap                = {0};       /* instance capabilities */
	CpaBoolean                 cnv_loose_mode     = CPA_FALSE;
	CpaBoolean                 cnvOpFlag          = CPA_FALSE;
	CpaBoolean                 cnvnrOpFlag        = CPA_FALSE;
	Cpa32U                     bufferMetaSize     = 0;
	int                        fd                 = 0;
	struct stat                stat;
	Cpa8U                     *dataInPtr          = NULL;
	Cpa32U                     bufferSize         = 0;
	Cpa32U                     dstBufferSize      = 0;
	Cpa8U                     *pBufferMetaSrc     = NULL;
	CpaBufferList             *pBufferListSrc     = NULL;
	Cpa32U                     numBuffers         = 1; /* only one buffer for this example */
	Cpa32U                     bufferListMemSize  = sizeof(CpaBufferList)
	                                                + (numBuffers * sizeof(CpaFlatBuffer));
	Cpa8U                     *pSrcBuffer         = NULL;
	Cpa8U                     *pBufferMetaDst     = NULL;
	Cpa8U                     *pBufferMetaDst2    = NULL;
	CpaBufferList             *pBufferListDst     = NULL;
	CpaBufferList             *pBufferListDst2    = NULL;
	Cpa8U                     *pDstBuffer         = NULL;
	Cpa8U                     *pDst2Buffer        = NULL;
	CpaFlatBuffer             *pFlatBuffer        = NULL;
	/* The following variables are allocated on the stack because we block until the callback
	   comes back. In a non-blocking approach, these should be dynamically allocated */
	struct completion_struct   complete;
	CpaDcRqResults             dcResults;


	/* initiating operational data */
	status = cpaDcQueryCapabilities(dcInstHandle, &cap);
	if (CPA_STATUS_SUCCESS != status) {
		printf("doCompression: error calling cpaDcQueryCapabilities with status=%d\n",
		       (int)status);
		return status;
	}
	cnv_loose_mode = (cap.compressAndVerifyStrict != CPA_TRUE) ? CPA_TRUE : CPA_FALSE;
	cnvnrOpFlag    = cap.compressAndVerifyAndRecover;
	if (cap.compressAndVerify == CPA_TRUE) {
		/* CNV loose is compress only */
		cnvOpFlag = (cnv_loose_mode == CPA_FALSE) ? CPA_TRUE : CPA_FALSE;
	} else {
		cnvOpFlag = CPA_FALSE;
	}
	if (cnvOpFlag == CPA_FALSE) {
		/* CNV recovery only possible when CNV enabled/present */
		cnvnrOpFlag = CPA_FALSE;
	}
	opData.compressAndVerify           = cnvOpFlag;
	opData.compressAndVerifyAndRecover = cnvnrOpFlag;
	opData.flushFlag                   = CPA_DC_FLUSH_FINAL;
	opData.inputSkipData.skipMode      = CPA_DC_SKIP_DISABLED;
	opData.outputSkipData.skipMode     = CPA_DC_SKIP_DISABLED;

	/*
	 * get metadata size. Different API implementations may require different amounts of
	 * space.
	 */
	status = cpaDcBufferListGetMetaSize(dcInstHandle, 1, &bufferMetaSize); /* only one buffer
										  in this case */
	printf("doCompression: status=%d bufferMetaSize=%d\n", (int)status, (int)bufferMetaSize);

	/* Preparing input data to be read into memory */
	fd = open(file, O_RDONLY);
	if (fd < 0) {
		printf("doCompression: open failed errno=%d\n", errno);
		return CPA_STATUS_FAIL;
	}
	ret = fstat(fd, &stat);
	if (ret < 0) {
		printf("doCompression: fstat failed errno=%d\n", errno);
		return CPA_STATUS_FAIL;
	}
	bufferSize = (Cpa32U) stat.st_size;

	printf("doCompression: (data in) file=%s size=%d\n", file, (int)bufferSize);

	dataInPtr = (Cpa8U *) mmap(NULL, (size_t)bufferSize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (-1 == (int64_t)dataInPtr) {
		printf("doCompression: mmap failed errno=%d\n", errno);
		return CPA_STATUS_FAIL;
	}
	ret = close(fd);
	if (ret < 0) {
		printf("doCompression: close failed errno=%d\n", errno);
		return CPA_STATUS_FAIL;
	}

	/* get destination buffer size for deflate operation (API version >= 2.5) */
	status = cpaDcDeflateCompressBound(dcInstHandle, huffType, bufferSize, &dstBufferSize);
	if (CPA_STATUS_SUCCESS != status) {
		printf("doCompression: error calling cpaDcDeflateCompressBound with status=%d\n",
		       (int)status);
		return CPA_STATUS_FAIL;
	}
	printf("doCompression: status=%d bufferSize=%d dstBufferSize=%d\n", (int)status,
	       (int)bufferSize, (int)dstBufferSize);

	/* allocate source buffer */
	if (CPA_STATUS_SUCCESS == status) {
		status = memAllocContig((void **)&pBufferMetaSrc, bufferMetaSize, 0, 1);
	}
	if (CPA_STATUS_SUCCESS == status) {
		status = memAlloc((void **)&pBufferListSrc, bufferListMemSize);
	}
	if (CPA_STATUS_SUCCESS == status) {
		status = memAllocContig((void **)&pSrcBuffer, bufferSize, 0, 1);
	}

	/* allocate destination buffer */
	if (CPA_STATUS_SUCCESS == status) {
		status = memAllocContig((void **)&pBufferMetaDst, bufferMetaSize, 0, 1);
	}
	if (CPA_STATUS_SUCCESS == status) {
		status = memAlloc((void **)&pBufferListDst, bufferListMemSize);
	}
	if (CPA_STATUS_SUCCESS == status) {
		status = memAllocContig((void **)&pDstBuffer, dstBufferSize, 0, 1);
	}

	/* copying input data into memory */
	memcpy(pSrcBuffer, dataInPtr, bufferSize);

	/* build source bufferList */
	pFlatBuffer = (CpaFlatBuffer *)(pBufferListSrc + 1); /* skip head, go directly to buffer 1 */

	pBufferListSrc->pBuffers         = pFlatBuffer;
	pBufferListSrc->numBuffers       = 1;
	pBufferListSrc->pPrivateMetaData = pBufferMetaSrc;
	pFlatBuffer->dataLenInBytes      = bufferSize;
	pFlatBuffer->pData               = pSrcBuffer;

	/* build destination bufferList */
	pFlatBuffer = (CpaFlatBuffer *)(pBufferListDst + 1); /* skip head, go directly to buffer 1 */

	pBufferListDst->pBuffers         = pFlatBuffer;
	pBufferListDst->numBuffers       = 1;
	pBufferListDst->pPrivateMetaData = pBufferMetaDst;
	pFlatBuffer->dataLenInBytes      = dstBufferSize;
	pFlatBuffer->pData               = pDstBuffer;

	/* init completion variable ...*/
	ret = COMPLETION_INIT(&complete);
	if (ret != 0) {
		printf("completionInit: sem_init failed errno=%d\n", errno);
		return CPA_STATUS_FAIL;
	}

	/* Let's compress the data */
	status = cpaDcCompressData2(dcInstHandle,
				    sessionHdl,
				    pBufferListSrc,   /* source buffer list */
				    pBufferListDst,   /* destination buffer list */
				    &opData,          /* operational data */
				    &dcResults,       /* results structure */
				    (void *)&complete /* data sent to the callback */
				    );
	if (CPA_STATUS_SUCCESS != status) {
		printf("doCompression: cpaDcCompressData2 failed. (status=%d)\n", (int)status);
	}

	/* wait for completion (blocking wait) */
	if (CPA_STATUS_SUCCESS == status) {
		if (!COMPLETION_WAIT(&complete)) {
			printf("doCompression: timeout or interruption in cpaDcCompressData2\n");
			status = CPA_STATUS_FAIL;
		}
	}

	/* we now check the results */
	if (CPA_STATUS_SUCCESS == status) {
		if (dcResults.status != CPA_DC_OK) {
			printf("doCompression: results not as expected status=%d\n",
			       (int)dcResults.status);
			status = CPA_STATUS_FAIL;
		} else {
			printf("=== DATA COMPRESSION ============\n");
			printf("data consumed %d\n", dcResults.consumed);
			printf("data produced %d\n", dcResults.produced);
			printf("adler checksum 0x%x\n", dcResults.checksum);
			printf("=================================\n");
		}
	}

	/* let's ensure we can decompress the original buffer */
	if (CPA_STATUS_SUCCESS == status) {
		/*
		 * Dst is not the Src buffer - updating the length with the amount of compressed
		 * data added tot he buffer
		 */
		pBufferListDst->pBuffers->dataLenInBytes = dcResults.produced;

		/* allocate memory for new destination buffer Dst2 */
		if (CPA_STATUS_SUCCESS == status) {
			status = memAllocContig((void **)&pBufferMetaDst2, bufferMetaSize, 0, 1);
		}
		if (CPA_STATUS_SUCCESS == status) {
			status = memAlloc((void **)&pBufferListDst2, bufferListMemSize);
		}
		if (CPA_STATUS_SUCCESS == status) {
			status = memAllocContig((void **)&pDst2Buffer, bufferSize, 0, 1);
		}

		/* build destination2 bufferList */
		pFlatBuffer = (CpaFlatBuffer *)(pBufferListDst2 + 1); /* skip head, go directly to
									 buffer 1 */

		pBufferListDst2->pBuffers         = pFlatBuffer;
		pBufferListDst2->numBuffers       = 1;
		pBufferListDst2->pPrivateMetaData = pBufferMetaDst2;
		pFlatBuffer->dataLenInBytes       = bufferSize;
		pFlatBuffer->pData                = pDst2Buffer;

		/* Let's decompress the data */
		status = cpaDcDecompressData2(dcInstHandle,
					      sessionHdl,
					      pBufferListDst,   /* source buffer list */
					      pBufferListDst2,  /* destination buffer list */
					      &opData,          /* operational data */
					      &dcResults,       /* results structure */
					      (void *)&complete
					     );
		if (CPA_STATUS_SUCCESS != status) {
			printf("doCompression: cpaDcDecompressData2 failed. (status=%d)\n",
			       (int)status);
		}

		/* wait for completion (blocking wait) */
		if (CPA_STATUS_SUCCESS == status) {
			if (!COMPLETION_WAIT(&complete)) {
				printf("doCompression: timeout or interruption in "
				       "cpaDcDecompressData2\n");
				status = CPA_STATUS_FAIL;
			}
		}

		/* check the results */
		if (CPA_STATUS_SUCCESS == status) {
			if (dcResults.status != CPA_DC_OK) {
				printf("doCompression: status not as expected (decomp) status=%d\n",
				       (int)status);
				status = CPA_STATUS_FAIL;
			} else {
				printf("=== DATA DECOMPRESSION ==========\n");
				printf("data consumed %d\n", dcResults.consumed);
				printf("data produced %d\n", dcResults.produced);
				printf("adler checksum 0x%x\n", dcResults.checksum);
				printf("=================================\n");
			}

			/* compare with original source buffer */
			if (0 == memcmp(pDst2Buffer, pSrcBuffer, bufferSize)) {
				printf("doCompression: output matches original input\n");
			} else {
				printf("doCompression: output does not match original input \n");
			}
		}
	}

	/* all callback functions have returned -> freeing all memory */
	ret = munmap(dataInPtr, bufferSize);
	if (ret < 0) {
		printf("doCompression: munmap failed errno=%d\n", errno);
		return CPA_STATUS_FAIL;
	}
	memFreeContig((void **)&pSrcBuffer);
	memFree((void **)&pBufferListSrc);
	memFreeContig((void **)&pBufferMetaSrc);
	memFreeContig((void **)&pDstBuffer);
	memFree((void **)&pBufferListDst);
	memFreeContig((void **)&pBufferMetaDst);
	memFreeContig((void **)&pDst2Buffer);
	memFree((void **)&pBufferListDst2);
	memFreeContig((void **)&pBufferMetaDst2);

	return status;
}
