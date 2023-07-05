#include "main.h"

CpaStatus memInit(void)
{
	return qaeMemInit();
}

void memDestroy(void)
{
	qaeMemDestroy();
}

CpaStatus memAllocContig(void **memAddr, Cpa32U sizeBytes, Cpa32U node, Cpa32U alignment)
{
	*memAddr = (void *)qaeMemAllocNUMA(sizeBytes, node, alignment);
	if (NULL == *memAddr) {
		return CPA_STATUS_RESOURCE;
	}
	return CPA_STATUS_SUCCESS;
}

void memFreeContig(void **memAddr)
{
	if (NULL != *memAddr) {
		qaeMemFreeNUMA(memAddr);
		*memAddr = NULL;
	}
}

CpaStatus memAlloc(void **memAddr, Cpa32U sizeBytes)
{
	*memAddr = malloc(sizeBytes);
	if (NULL == *memAddr) {
		return CPA_STATUS_RESOURCE;
	}
	return CPA_STATUS_SUCCESS;
}

void memFree(void **memAddr)
{
	if (NULL != *memAddr) {
		free(*memAddr);
		*memAddr = NULL;
	}
}

void memFreeNUMA(void **memAddr)
{
	qaeMemFreeNUMA(memAddr);
}
