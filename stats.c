#include <stdlib.h>

static int cmp(const void *p1, const void *p2)
{
	unsigned long *a1 = (unsigned long *)p1;
	unsigned long *a2 = (unsigned long *)p2;

	if (*a1 < *a2)
		return -1;
	if (*a1 > *a2)
		return 1;

	return 0;
}

unsigned long stats_best(unsigned long *times, unsigned int entries)
{
	qsort(times, entries, sizeof(*times), cmp);

#if 0
	for (i = 0; i < entries;i++)
		printf("%lu ", times[i]);
	printf("\n");
#endif

	/* 90th percentile */
	return times[9*entries/10];
}
