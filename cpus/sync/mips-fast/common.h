#include "pipereg.h"
#include <execinfo.h>
#include <signal.h>
#ifdef MIPC_DEBUG
#define DDBG fprintf(stdout, "%s %d\n", __FILE__, __LINE__)
#else
#define DDBG
#endif

extern Pipereg *pipeline;
extern FILE *debugLog;

void MSG(char *a);
void handler(int sig);
