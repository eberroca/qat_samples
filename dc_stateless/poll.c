#include "main.h"


static volatile int gPollingDc = 0;

void turnPollingOff(void)
{
	gPollingDc = 0;
}

void *pollingThreadFunc(CpaInstanceHandle dcInstHandle)
{
	int ret;


	gPollingDc = 1;
	while (gPollingDc) {
		icp_sal_DcPollInstance(dcInstHandle, 0);

		ret = sleepForMs(10);
		if (0 != ret) {
			printf("nanosleep failed with code %d\n", ret);
		}
	}
	pthread_exit(NULL);
}
