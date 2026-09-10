/*
 * tfaOsal.c
 *
 *  Operating specifics
 */


#ifndef TFAOSAL_H_
#define TFAOSAL_H_

int tfaosal_filewrite(const char *fullname, unsigned char *buffer, int filelenght );

#if defined (WIN32) || defined(_X64)
//suppress warnings for unsafe string routines.
#pragma warning(disable : 4996)
char *basename(const char *fullpath);
#define bzero(ADDR,SZ)	memset(ADDR,0,SZ)
#define isblank(C) (C==' '||C=='\t')
#endif

void tfaRun_Sleepus(int us);

/*
 * Read file
 */
int  tfaReadFile(char *fname, void **buffer);

#endif
