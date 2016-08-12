#include <stdbool.h>
#include "adrestia.h"
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

struct wake_data {
	struct timeval tv;
	int start_fds[2];
	int result_fds[2];
};

static void *wake_func(void *arg)
{
	struct thread_data *td = arg;
	struct wake_data *wd = td->arg;
	struct timeval now, *tv, res;
	char data;

	while (1) {
		if (read(wd->start_fds[0], &data, 1) != 1) {
			fprintf(stderr, "error waiting for wakeup\n");
			abort();
		}

		gettimeofday(&now, NULL);
		tv = &wd->tv;

		timersub(&now, tv, &res);

		*tv = res;

		if (write(wd->result_fds[1], &data, 1) != 1) {
			fprintf(stderr, "error waking master\n");
			abort();
		}

		pthread_testcancel();
	}
	
	return NULL;
}

static unsigned long wake_tasks(unsigned int nr_threads)
{
	struct timeval tmp, res;
	struct thread *t;
	int i;

	for (i = 0; i < nr_threads;i++) {
		struct wake_data *wd;
		struct timeval *tv;
		char data;

		t = &threads[i];
		wd = t->td->arg;	
		tv = &wd->tv;

		gettimeofday(tv, NULL);
		write(wd->start_fds[1], &data, 1);
	}

	res.tv_sec = 0;
	res.tv_usec = 0;

	for (i = 0; i < nr_threads; i++) {
		struct wake_data *wd;
		char data;

		t = &threads[i];
		wd = t->td->arg;

		if (read(wd->result_fds[0], &data, 1) != 1) {
			perror("read");
			abort();
		}

		tmp = res;
		timeradd(&tmp, &wd->tv, &res);
	}

	return (res.tv_sec * 1000000 + res.tv_usec) / nr_threads;
}

static void wake_ctor(struct thread *t, void *arg)
{
	struct wake_data *wd;

	wd = calloc(1, sizeof(*wd));
	if (!wd) {
		perror("calloc");
		abort();
	}

	t->td->arg = wd;

	if (pipe(wd->start_fds) || pipe(wd->result_fds)) {
		perror("pipe");
		abort();
	}
}

static void wake_dtor(struct thread *t)
{
	struct wake_data *wd = t->td->arg;
	int i;

	for (i = 0; i < 2; i++) {
		close(wd->start_fds[i]);
		close(wd->result_fds[i]);
	}

	free(wd);
}

static unsigned long
measure_wakeup(unsigned int nr_threads, unsigned int nr_loops,
	       unsigned long (*func)(unsigned int))
{
	unsigned long *wake_times;
	int i;

	wake_times = calloc(nr_loops, sizeof(*wake_times));
	if (!wake_times) {
		perror("calloc");
		abort();
	}

	thread_startup(nr_threads, NULL, wake_ctor, wake_func);

	for (i = 0; i < nr_loops; i++)
		wake_times[i] = func(nr_threads);

	for (i = 0; i < nr_threads; i++) {
		struct thread *t = &threads[i];
		pthread_cancel(t->tid);
	}

	thread_teardown(nr_threads, wake_dtor);

	return stats_best(wake_times, nr_loops);
}

static unsigned long wake_tasks_periodic(unsigned int nr_threads)
{
	usleep(arrival_rate);

	return wake_tasks(nr_threads);
}

/*
 * Measure the cost of waking up a single thread.
 */
int test_wakeup_single(void)
{
	unsigned long cost;

	cost = measure_wakeup(1, num_loops*num_threads, wake_tasks);
	printf("wakeup cost (single): %luus\n", cost);

	return 0;
}

/*
 * Periodically wake up threads, which then immediately block waiting
 * for the next wakeup to arrive. %arrival_rate specifies the waiting
 * time between wakeups.
 */
int test_wakeup_periodic(void)
{
	unsigned long cost;

	cost = measure_wakeup(num_threads, num_loops, wake_tasks_periodic);
	printf("wakeup cost (periodic, %uus): %luus\n", arrival_rate, cost);

	return 0;
}
