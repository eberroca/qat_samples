
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <cpa.h>
#include <qae_mem.h>
#include <qae_mem_utils.h>
#include <stdio.h>
#include <cpa.h>
#include <cpa_dc.h>
#include <icp_sal_user.h>
#include <icp_sal_poll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>


#define MAX_INSTANCES 64
#define SAMPLE_MAX_BUFF 1024


struct completion_struct
{
	sem_t semaphore;
};

#define COMPLETION_INIT(c) sem_init(&((c)->semaphore), 0, 0)

#define COMPLETION_WAIT(c) (sem_wait(&((c)->semaphore)) == 0)

#define COMPLETE(c) sem_post(&((c)->semaphore))


int sleepForMs(uint32_t ms);

CpaStatus memInit(void);

void memDestroy(void);

CpaStatus memAllocContig(void **memAddr, Cpa32U sizeBytes, Cpa32U node, Cpa32U alignment);

void memFreeContig(void **memAddr);

CpaStatus memAlloc(void **memAddr, Cpa32U sizeBytes);

void memFree(void **memAddr);

void memFreeNUMA(void **memAddr);

CpaStatus doCompression(const char *file, CpaInstanceHandle dcInstHandle,
			CpaDcSessionHandle sessionHdl, CpaDcHuffType huffType);

void turnPollingOff(void);

void *pollingThreadFunc(CpaInstanceHandle dcInstHandle);

void dcCallback(void *pCallbackTag, CpaStatus status);
