/**********************************************
 * Dawn Light Player
 *
 *   pthread.c
 *
 * Created by kf701
 * 21:41:52 03/13/08 CST
 *
 * $Id: my_pthread.c 38 2008-04-08 10:38:59Z kf701 $
 **********************************************/

#if defined(__MINGW32__)

#include <process.h>
#include <time.h>
#include <sys/time.h>
#include "my_pthread.h"

/*-----------------------------------------------------------------------------
 *  pthread mutex wrapper
 *-----------------------------------------------------------------------------*/
int pthread_mutex_init(pthread_mutex_t * mutex, pthread_mutexattr_t * attr)
{
	InitializeCriticalSection(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t * mutex)
{
	DeleteCriticalSection(mutex);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t * mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t * mutex)
{
	LeaveCriticalSection(mutex);
	return 0;
}

/*-----------------------------------------------------------------------------
 *  pthread cond wrapper
 *-----------------------------------------------------------------------------*/
int pthread_cond_init(pthread_cond_t * cv, const pthread_condattr_t * attr)
{
	cv->events_[SIGNAL] = CreateEvent(NULL, FALSE, FALSE, NULL);
	cv->events_[BROADCAST] = CreateEvent(NULL, TRUE, FALSE, NULL);
	return 0;
}

int pthread_cond_destroy(pthread_cond_t * cv)
{
	CloseHandle(cv->events_[SIGNAL]);
	CloseHandle(cv->events_[BROADCAST]);
	return 0;
}

int pthread_cond_wait(pthread_cond_t * cv, pthread_mutex_t * mutex)
{
	WaitForMultipleObjects(2, cv->events_, FALSE, INFINITE);
	EnterCriticalSection(mutex);
	return 0;
}

int pthread_cond_timedwait(pthread_cond_t * cv, pthread_mutex_t * mutex,
			   struct timespec *ts)
{
	uint32_t t, second, usecond;
	struct timeval tv;
	gettimeofday(&tv, NULL);

	second = ts->tv_sec - tv.tv_sec;
	if ((ts->tv_nsec / 1000) >= tv.tv_usec)
		usecond = (ts->tv_nsec / 1000 - tv.tv_usec);
	else
		usecond = 1000000 - (tv.tv_usec - ts->tv_nsec / 1000);

	t = second * 1000 + usecond / 1000;
	WaitForMultipleObjects(2, cv->events_, FALSE, t);

	EnterCriticalSection(mutex);

	return 0;
}

int pthread_cond_signal(pthread_cond_t * cv)
{
	PulseEvent(cv->events_[SIGNAL]);
	return 0;
}

int pthread_cond_broadcast(pthread_cond_t * cv)
{
	PulseEvent(cv->events_[BROADCAST]);
	return 0;
}

/*-----------------------------------------------------------------------------
 *  pthread control wrapper
 *-----------------------------------------------------------------------------*/
int pthread_create(pthread_t * thread, const pthread_attr_t * attr,
		   void *(*func) (void *), void *arg)
{
	HANDLE ret;
	ret = _beginthreadex(NULL, 0, (start_address) func, NULL, 0, thread);
	if (0 == ret)
		return -1;
	if (thread)
		*thread = ret;
	return 0;		/* successful */
}

void pthread_exit(void *ptr)
{
	_endthreadex(0);
}

/*-----------------------------------------------------------------------------
 *  unix misc wrapper
 *-----------------------------------------------------------------------------*/
int sleep(int t)
{
	Sleep(t * 1000);
	return 0;
}

int usleep(int t)
{
	Sleep(t / 1000);
	return 0;
}

int pause(void)
{
	while (1)
		Sleep(1000 * 1000);
	return 0;
}

#endif
