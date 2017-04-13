#include "pipereg.h"
#include <execinfo.h>
#include <signal.h>

#ifndef _COMMON_H
#define _COMMON_H

#ifdef MIPC_DEBUG
#define DDBG /*fprintf(stdout, "%s %d\n", __FILE__, __LINE__)*/
#else
#define DDBG
#endif

#ifdef MIPC_DEBUG
#define DBG(a) fprintf a
#else
#define DBG(a)
#endif

#define INLOG(a) {fprintf a ; fflush(inlog); }

extern Pipereg *pipeline;
extern FILE *debugLog;
extern FILE *inlog;

extern int cycleId;

void MSG(char *a);
void handler(int sig);

#endif
