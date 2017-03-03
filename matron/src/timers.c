/*
  timer.c
  
  accurate timers using pthreads and clock_nanosleep.

  TODO: need thread synchronization if we want to change timer params on the fly
*/

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "events.h"
#include "timers.h"

#define MAX_NUM_TIMERS_OK 32

enum {
  RUNNING,
  STOPPED
};

const int MAX_NUM_TIMERS = MAX_NUM_TIMERS_OK;
struct timer {
  int idx;         // timer index
  int state;       // running/stoppeb*d status
  uint64_t count;  // total iterations ( <0 -> infinite )
  uint64_t cur;    // current count of iterations
  uint64_t time;   // current time (in nsec)
  uint64_t delta;  // current delta (in nsec)
  pthread_t tid;   // thread id
};

struct timer timers[MAX_NUM_TIMERS_OK];

//---------------------------
//---- static declarations
static void timer_handle_error(int code, const char* msg) {
  printf("error code: %d ; message: \"%s\"", code, msg); fflush(stdout);
}

static void timer_init(struct timer *t, uint64_t nsec, int count);
static void timer_set_current_time(struct timer *t);
static void* timer_thread_start(void* timer);
static void timer_bang(struct timer *t);
static void timer_reset(struct timer* t) { t->cur = 0; }

//------------------------
//---- extern definitions

 void timers_init(void) {
   for(int i=0; i<MAX_NUM_TIMERS_OK; i++) {
	 timers[i].state = STOPPED;
   }
}

void timer_start(int idx, double seconds, int count) {
  uint64_t nsec = (uint64_t) (seconds * 1000000000.0);
  
  if(idx >= 0 && idx < MAX_NUM_TIMERS_OK) {
	if(timers[idx].state == STOPPED) {
	  //	  printf("timer_add(): OK: index %d, period %d nsec, count %d \r\n",
	  //			 idx, nsec, count);  fflush(stdout);
	  timers[idx].idx = idx;
	  timer_init(&timers[idx], nsec, count);
	  timer_reset(&timers[idx]);
	} else {
	  // printf("timer_add(): aborting; timer is already running \n");
	  // fflush(stdout);
	}
  } else {
	printf("invalid timer index, not added. max count of timers is %d\n",
		   MAX_NUM_TIMERS_OK);  fflush(stdout);
  }
}
			   
//------------------------
//---- static definitions

void timer_init(struct timer *t, uint64_t nsec, int count) {
  int res;
  pthread_attr_t attr;
  
  res = pthread_attr_init(&attr);
  if(res != 0) {
	timer_handle_error(res, "pthread_attr_init");
	return;
  }
  // set other thread attributes here... 

  t->delta = nsec;
  t->count = count;
  
  res = pthread_create(&(t->tid), &attr, &timer_thread_start, (void*)t);
  if(res !=0) {
	timer_handle_error(res, "pthread_create");
	return;
  }
  else {
	// set thread priority to realtime
	struct sched_param param;
	param.sched_priority = sched_get_priority_max (SCHED_RR);
	res = pthread_setschedparam (t->tid, SCHED_RR, &param);
	if(res !=0) {
	  timer_handle_error(res, "pthread_setschedparam");
	  return;
	}
	t->state = RUNNING;
  }
  
}

void* timer_thread_start(void* timer) {
  struct timer *t = (struct timer*) timer;

  timer_set_current_time(t);
  
  if(t->count < 0) {
	while(1) {
	  timer_bang(t);
	}
  } else {
	while(t->cur < (t->count)) {
	  timer_bang(t);
	}
  }
}

void timer_set_current_time(struct timer *t) {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  t->time = (uint64_t) ((1000000000 * (int64_t)time.tv_sec) + (int64_t)time.tv_nsec);
}

void timer_bang(struct timer *t) {
  t->cur += 1;
  event_post(EVENT_TIMER, &(t->idx), &(t->cur));

  // with nanosleep(), thread can be preempted while sleeping.
  // so for accuracy, tell system clock to wake us at absolute time:
  struct timespec ts;
  t->time += t->delta;
  ts.tv_sec = (time_t) t->time / 1000000000;
  ts.tv_nsec = (long) t->time % 1000000000;
  clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
}

void timer_wait(int idx) {
  pthread_join(timers[idx].tid, NULL);
}

void timer_stop(int idx) {
  if( timers[idx].state == STOPPED) {
	//	printf("timer_stop(): nothing to do (timer is stopped)\n");
	fflush(stdout);
	return;
  }
  if(idx >=0 && idx < MAX_NUM_TIMERS_OK) { 
	int ret = pthread_cancel(timers[idx].tid);
	if(ret) { 
	  printf("timer_stop(): pthread_cancel() failed; error: 0x%08x\r\n", ret);
	  fflush(stdout);
	} else {
	  timers[idx].state = STOPPED;
	  //	  printf("timer_stop(): OK (timer %d)\n", idx); fflush(stdout);
	}
  } else {
	printf("invalid timer index, not stopped. max count of timers is %d\n",
		   MAX_NUM_TIMERS_OK );  fflush(stdout);
  }
}

#undef MAX_NUM_TIMERS_OK
