/*
 * Copyright (C) 2016 Matt Fleming
 */
#include "adrestia.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
	const char usage_str[] =
"Usage: [OPTION] [TESTNAME]\n"
"\n"
"\t-a\tarrival time (microseconds)\n"
"\t-l\tnumber of loops\n"
"\t-L\tlist tests\n"
"\t-s\tservice time (microseconds)\n"
"\t-t\tthread count\n";

	fprintf(stderr, usage_str);
}

struct test {
	const char *name;
	int (*func)(void);
};

static struct test tests[] = {
	{ "wakeup-single", test_wakeup_single },
	{ "wakeup-periodic", test_wakeup_periodic },
};

#define for_each_test(i, t)		\
	for (i = 0, t = &tests[0]; i < ARRAY_SIZE(tests); i++, t++)

static void list_tests(void)
{
	struct test *t;
	int i;

	for_each_test(i, t)
		printf("%s\n", t->name);
}

long num_cpus;
long num_threads;

/* Arrival rate in microseconds */
#define ARRIVAL_RATE_US	10
int arrival_rate = ARRIVAL_RATE_US;

/* Task service time in microseconds */
#define SERVICE_TIME_US 50
int service_time = SERVICE_TIME_US;

#define NR_LOOPS	10000
int num_loops = NR_LOOPS;

struct thread *threads;

void thread_startup(unsigned int nr_threads, void *arg,
		    void (*ctor)(struct thread *t, void *arg),
		    void *(*thread_func)(void *arg))
{
	int i;

	threads = calloc(nr_threads, sizeof(*threads));
	if (!threads) {
		perror("calloc");
		abort();
	}

	for (i = 0; i < nr_threads; i++) {
		struct thread_data *td;
		struct thread *t;

		t = &threads[i];

		td = calloc(1, sizeof(*td));
		if (!td) {
			perror("calloc");
			abort();
		}

		td->cpu = i;
		t->td = td;

		ctor(t, arg);

		if (pthread_create(&t->tid, NULL, thread_func, t->td)) {
			perror("pthread_create");
			abort();
		}
	}
}

void thread_teardown(unsigned int nr_threads,
		    void (*dtor)(struct thread *t))
{
	int dead_threads = 0;
	int i;

	while (dead_threads < nr_threads) {
		for (i = 0; i < nr_threads; i++) {
			struct thread *t;

			t = &threads[i];

			if (t->dead)
				continue;

			if (pthread_tryjoin_np(t->tid, NULL))
				continue;

			t->dead = 1;
			dead_threads++;

			dtor(t);
		}
	}

	free(threads);
}

int main(int argc, char **argv)
{
	char *testname = NULL;
	struct test *t;
	bool found = false;
	int opt;
	int i;

	num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	num_threads = 1;

	while ((opt = getopt(argc, argv, "a:l:Ls:t:")) != -1) {
		switch (opt) {
		case 'a':
			arrival_rate = atoi(optarg);
			break;
		case 'l':
			num_loops = atoi(optarg);
			break;
		case 'L':
			list_tests();
			exit(0);
			break;
		case 's':
			service_time = atoi(optarg);
			break;
		case 't':
			num_threads = atoi(optarg);
			break;
		default: /* ? */
			usage();
			exit(1);
		}
	}

	testname = argv[optind];

	for_each_test(i, t) {
		if (testname && strcmp(t->name, testname))
			continue;

		found = true;
		t->func();
	}

	if (!found) {
		fprintf(stderr, "Invalid testname\n");
		exit(1);
	}

	return 0;
}
