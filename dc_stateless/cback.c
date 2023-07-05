#include "main.h"

void dcCallback(void *pCallbackTag, CpaStatus status)
{
	printf("callback called with status=%d\n", (int)status);

	if (NULL != pCallbackTag) {
		COMPLETE((struct completion_struct *)pCallbackTag);
	}
}
