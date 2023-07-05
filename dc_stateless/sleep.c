#include "main.h"

int sleepForMs(uint32_t ms)
{
	int              ret       = 0;
	struct timespec  resTime;
	struct timespec  remTime;


	resTime.tv_sec  = ms / 1000;
	resTime.tv_nsec = (ms % 1000) * 1000000;

	/* sleep for at last ms, even if interrupted */
	do {
		ret     = nanosleep(&resTime, &remTime);
		resTime = remTime;
	} while ((0 != ret) && (errno == EINTR));

	return ret;
}
