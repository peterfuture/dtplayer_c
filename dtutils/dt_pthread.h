/**********************************************
 * Dawn Light Player
 *
 *   pthread.h
 *
 * Created by kf701
 * 21:41:49 03/13/08 CST
 *
 * $Id: my_pthread.h 32 2008-04-03 07:39:58Z kf701 $
 **********************************************/

#ifndef  KF701_PTHREAD_INC
#define  KF701_PTHREAD_INC

#if defined(__MINGW32__)

#include <stdint.h>
#include <windows.h>

struct timespec {
	unsigned int tv_sec;	/* Seconds.  */
	long int tv_nsec;	/* Nanoseconds.  */
};

typedef struct {
	enum {
		SIGNAL = 0,
		BROADCAST = 1,
		MAX_EVENTS = 2
	} xx;

	HANDLE events_[MAX_EVENTS];

} pthread_cond_t;

typedef CRITICAL_SECTION pthread_mutex_t;

/* attr not implement, don't use it now */
typedef int pthread_attr_t;
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;

int pthread_mutex_init(pthread_mutex_t * mutex, pthread_mutexattr_t * attr);
int pthread_mutex_destroy(pthread_mutex_t * mutex);
int pthread_mutex_lock(pthread_mutex_t * mutex);
int pthread_mutex_unlock(pthread_mutex_t * mutex);

int pthread_cond_init(pthread_cond_t * cv, const pthread_condattr_t *);
int pthread_cond_destroy(pthread_cond_t * cv);
int pthread_cond_wait(pthread_cond_t * cv, pthread_mutex_t * mutex);
int pthread_cond_timedwait(pthread_cond_t * cv, pthread_mutex_t * mutex,
			   struct timespec *ts);
int pthread_cond_signal(pthread_cond_t * cv);
int pthread_cond_broadcast(pthread_cond_t * cv);

typedef void (__stdcall * start_address) (void *);
typedef unsigned long pthread_t;

int pthread_create(pthread_t * thread, const pthread_attr_t * attr,
		   void *(*func) (void *), void *arg);
void pthread_exit(void *ptr);

int sleep(int t);
int usleep(int t);
int pause(void);

#else /* WIN32 || MINGW32 */
#include <pthread.h>
#endif

#endif /* ----- #ifndef KF701_PTHREAD_INC  ----- */
