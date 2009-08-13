#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char		*progname;
static const key_t	key_astribanks = 0xAB11A0;
static int		debug;

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-d] [-a] [-r]\n", progname);
	exit(1);
}

static int absem_get(int createit)
{
	int	flags = (createit) ? IPC_CREAT | 0644 : 0;
	int	absem;

	if((absem = semget(key_astribanks, 1, flags)) < 0)
		absem = -errno;
	return absem;
}

static int absem_touch(void)
{
	int		absem;

	if((absem = absem_get(1)) < 0) {
		perror(__FUNCTION__);
		return absem;
	}
	if(semctl(absem, 0, SETVAL, 0) < 0) {
		perror("SETVAL");
		return -errno;
	}
	if(debug)
		fprintf(stderr, "%s: touched absem\n", progname);
	return 0;
}

static int absem_remove(void)
{
	int	absem;

	if((absem = absem_get(0)) < 0) {
		if(absem == -ENOENT) {
			if(debug)
				fprintf(stderr, "%s: absem already removed\n", progname);
			return 0;
		}
		perror(__FUNCTION__);
		return absem;
	}
	if(semctl(absem, 0, IPC_RMID, 0) < 0) {
		perror("RMID");
		return -errno;
	}
	if(debug)
		fprintf(stderr, "%s: removed absem\n", progname);
	return 0;
}

static int absem_detected(void)
{
	int	absem;

	if((absem = absem_get(0)) < 0) {
		if(debug)
			fprintf(stderr, "%s: absem does not exist\n", progname);
		return absem;
	}
	if(debug)
		fprintf(stderr, "%s: absem exists\n", progname);
	return 0;
}

int main(int argc, char *argv[])
{
	const char	options[] = "darh";
	int		val;

	progname = argv[0];
	while (1) {
		int	c;

		c = getopt (argc, argv, options);
		if (c == -1)
			break;

		switch (c) {
			case 'd':
				debug++;
				break;
			case 'a':
				if((val = absem_touch()) < 0) {
					fprintf(stderr, "%s: Add failed: %d\n", progname, val);
					return 1;
				}
				return 0;
			case 'r':
				if((val = absem_remove()) < 0) {
					fprintf(stderr, "%s: Remove failed: %d\n", progname, val);
					return 1;
				}
				return 0;
			case 'h':
			default:
				fprintf(stderr, "Unknown option '%c'\n", c);
				usage();
		}
	}
	val = absem_detected();
	return (val == 0) ? 0 : 1;
}
