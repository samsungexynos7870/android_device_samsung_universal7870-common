/*
 * tfaOsal.c
 *
 *  Operating specifics
 */
#include "tfaOsal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#if (defined(WIN32) || defined(_X64))
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "dbgprint.h"
#include "tfaContUtil.h"
#include "tfaOsal.h"

#if defined(WIN32) || defined(_X64)
char *basename(const char *fullpath) {
	static char name[_MAX_FNAME];
	static char ext[_MAX_FNAME];
	static char *fullname;

	/* if no path in filename windows looses the extension */
	if ( strchr(fullpath,'\\') || strchr(fullpath,'/') ) {
		_splitpath_s(fullpath, NULL, 0, NULL, 0, name, _MAX_FNAME, ext, _MAX_FNAME) == 0 ? name : NULL;

		fullname = strcat(name, ext);

		return fullname;
	}

	fullname = (char *)fullpath;
	/* no path in filename */
	return fullname;
}

#endif

int tfaosal_filewrite(const char *fullname, unsigned char *buffer, int filelenght )
{
	FILE *f;
	int c=0;

	f = fopen( fullname, "wb");
		if (!f)
		{
			PRINT("%s, Unable to open %s\n", __FUNCTION__, fullname);
			return Tfa98xx_Error_Other;
		}
		c = (int)fwrite( buffer, 1, filelenght, f );
		fclose(f);
		if ( c == filelenght)
			return Tfa98xx_Error_Ok;
		else {
			PRINT("%s: File write error %s %d %d \n", __FUNCTION__, fullname, c, filelenght);
		return Tfa98xx_Error_Other;
		}
}

static int fsize(const char *name)
{
	struct stat st;
	stat(name, &st);
	return st.st_size;
}

/*
 * check file name
 * allocate size
 * read in file
 * return size or 0 on failure
 * NOTE: the caller must free the buffer if size is non-0
 */
int  tfaReadFile(char *fname, void **buffer) {
	int size;
	size_t bytes_read;
	FILE *f;

	size = fsize(fname);
	if (size <= 0) {
		ERRORMSG("Can't open %s.\n", fname);
		return 0;
	}

	*buffer = malloc(size);
	if ( !*buffer ) {
		ERRORMSG("Can't allocate %d bytes.\n", size);
		return 0;
	}

	f=fopen(fname, "rb");
	if ( !f ) {
		ERRORMSG("Error when reading %s.\n", fname);
		return 0;
	}

	bytes_read = fread(*buffer, size, 1, f);
	if (bytes_read == 0) {
		fclose(f);
		ERRORMSG("Nothing read when reading %s.\n", fname);
	return 0;
	}

	fclose(f);

	return size;
}

/*
 * sleep requested amount of micro seconds
 */
void tfaRun_Sleepus(int us)
{
#if (defined(WIN32) || defined(_X64))
	int rest;

	rest = us%1000;
	if (rest)
		us += 1000; // round up for windows TODO check usec in windows
	Sleep(us/1000); // is usinterval
#elif (defined(__REDLIB__))
	// TODO !need sleep here
#else
    usleep(us); // is usinterval
#endif
}
