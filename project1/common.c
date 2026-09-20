#include "common.h"

void err_n_die(const char *fmt, ...)
{
	int errno_save;
	va_list ap;

	// all system calls can set errno, so we need to save it now
	errno_save = errno;

	// print out the fmt+args to standard error
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	fflush(stderr);

	// print out error message is errno was set.
	if (errno_save != 0)
	{
		fprintf(stderr, "(errno = %d) : %s\n", errno_save, strerror(errno_save));
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	va_end(ap);

	// this is the ..and_die part. Exit with an error.
	exit(1);
}