#include "pipereg.h"
#include <execinfo.h>
#include <signal.h>
#define DDBG fprintf(stdout, "%s %d\n", __FILE__, __LINE__);

extern Pipereg *pipeline;
extern FILE *debugLog;

void MSG(char *a);
void handler(int sig);
