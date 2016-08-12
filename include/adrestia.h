#ifndef _ADRESTIA_H_
#define _ADRESTIA_H_

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

extern int test_wakeup_single(void);
extern int test_wakeup_periodic(void);

struct thread_data {
	unsigned int cpu;
	pthread_barrier_t *barrier;
	void *arg;
};

struct thread {
	pthread_t tid;
	struct thread_data *td;
	unsigned int dead;
};

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

extern long num_cpus;
extern long num_threads;
extern int arrival_rate;
extern int service_time; 
extern int num_loops;

extern struct thread *threads;

extern void thread_startup(unsigned int nr_threads, void *arg,
			   void (*ctor)(struct thread *t, void *),
			   void *(*thread_func)(void *));
extern void thread_teardown(unsigned int num,
			    void (*dtor)(struct thread *t));

static inline void thread_pin(unsigned int cpu)
{
	cpu_set_t mask;

	/*
	 * Make sure that cpu_set_t is large enough to cover cpus.
	 */
	if (sizeof(cpu_set_t) < num_cpus)
		abort();

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask)) {
		perror("sched_setaffinity");
		abort();
	}
}

static inline void thread_unpin(void)
{
	cpu_set_t mask;
	int i;

	CPU_ZERO(&mask);

	for (i = 0; i < num_cpus; i++)
		CPU_SET(i, &mask);

	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask)) {
		perror("sched_setaffinity");
		abort();
	}
}

/* stats.c */
extern unsigned long stats_best(unsigned long *times, unsigned int entries);

#endif /* _ADRESTIA_H_ */
