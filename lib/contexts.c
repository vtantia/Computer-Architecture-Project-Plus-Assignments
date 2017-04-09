/*************************************************************************
 *
 *  (c) 1998-1999 Cornell University
 *  Computer Systems Laboratory
 *  Ithaca, NY 14853.
 *  All Rights Reserved
 *
 *
 *  (c) 1996-1998 California Institute of Technology
 *  Department of Computer Science
 *  Pasadena, CA 91125.
 *  All Rights Reserved
 *
 *  $Id: contexts.c,v 1.1.1.1 2006/05/23 13:53:59 mainakc Exp $
 *
 *************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include "contexts.h"

struct process_record {
  context_t c;
};

/* current process */
#ifdef __ia64__
context_t *current_process = NULL;
static context_t *terminated_process = NULL;
ucontext_t dummy;
context_t *old_process = NULL;

__inline__ unsigned long
GetSP (void)
{
        unsigned long result;

        __asm__ __volatile__("mov %0=r12;;" : "=r"(result) :: "memory");
        return result;
}
#else
process_t *current_process = NULL;
static process_t *terminated_process = NULL;
#endif

#ifdef FAIR

static struct itimerval mt;	/* the timer for the main thread */
static int unfair = 0;
static int enable_mask;
static int disable_mask;

static int mt_in_cs = 0;	/* main thread in cs */
static int mt_pending = 0;	/* pending thread 0 */
static int mt_interrupted = 0;  /* main thread interrupted? */


#define SLICE_SEC  0		/* time slice in */
#define SLICE_USEC 100		/* secs and usecs */


#define IN_CS(proc) (proc ? (proc)->c.in_cs : mt_in_cs)

#define PENDING(proc) (proc ? (proc)->c.pending : mt_pending)

#define INTERRUPTED(proc) (proc ? (proc)->c.interrupted : mt_interrupted)

#define SET_INTRPT(proc) do { if (proc) (proc)->c.interrupted = 1; else mt_interrupted = 1; } while(0)

#define CLR_INTRPT(proc) do { if (proc) (proc)->c.interrupted = 0; else mt_interrupted = 0; } while(0)

#define ENTER_CS(proc) do {                  	\
		         if (proc)           	\
		           (proc)->c.in_cs = 1;	\
		         else                	\
		           mt_in_cs = 1;     	\
		       } while (0)

#define LEAVE_CS(proc) do {                                         	\
		         if (proc) {                                	\
		           (proc)->c.in_cs = 0;                       	\
		           if ((proc)->c.pending) {                   	\
		             (proc)->c.pending = 0;                   	\
		             setitimer (ITIMER_VIRTUAL, &mt, NULL); 	\
		             context_timeout ();                    	\
		           }                                        	\
		         }                                          	\
		         else {                                     	\
		           mt_in_cs = 0;                            	\
		           if (mt_pending) {                        	\
		             mt_pending = 0;                        	\
		             setitimer (ITIMER_VIRTUAL, &mt, NULL); 	\
		             context_timeout ();                    	\
		           }                                        	\
		         }                                          	\
		       } while(0)

#define MAKE_PENDING(proc)  do {                     	\
			      if (proc)              	\
			        (proc)->c.pending = 1; 	\
			      else                   	\
			        mt_pending = 1;      	\
			    } while (0)

/*------------------------------------------------------------------------
 *
 *  Handle timer interrupt
 *
 *------------------------------------------------------------------------
 */
static void interrupt_handler (int sig)
{
#if defined(__hppa) && defined (__hpux) || \
    defined(__sparc__) && defined(__svr4__)
  /* running a sucky os */
  signal (SIGVTALRM, interrupt_handler);
#endif
  if (IN_CS (current_process))
    MAKE_PENDING(current_process);
  else if (INTERRUPTED (current_process)) {
    CLR_INTRPT (current_process);
    setitimer (ITIMER_VIRTUAL, &mt, NULL);
    context_timeout ();
  }
  else 
    SET_INTRPT (current_process);
}

/*------------------------------------------------------------------------
 *
 * Initialize timer data structure
 *
 *------------------------------------------------------------------------
 */
static void context_internal_init (void)
{
  static int first = 1;
  if (first && !unfair) {
    first = 0;
    mt.it_interval.tv_sec = 0;
    mt.it_interval.tv_usec = 0;
    mt.it_value.tv_sec = SLICE_SEC;
    mt.it_value.tv_usec = SLICE_USEC;
    signal (SIGVTALRM, interrupt_handler);
    setitimer (ITIMER_VIRTUAL, &mt, NULL);
  }
}
#endif


/*------------------------------------------------------------------------
 *  
 *  context_unfair --
 *
 *      Return back to unfair scheduling
 *
 *------------------------------------------------------------------------
 */
void context_unfair (void)
{
#ifdef FAIR
  struct itimerval t;
  unfair = 1;
  t.it_interval.tv_sec = 0;  t.it_interval.tv_usec =0;
  t.it_value.tv_sec = 0; t.it_value.tv_usec = 0;
  setitimer (ITIMER_VIRTUAL, &t, NULL);	/* disable timer interrupts */
#endif
}

/*------------------------------------------------------------------------
 *  
 *  context_fair --
 *
 *      Return back to unfair scheduling
 *
 *------------------------------------------------------------------------
 */
void context_fair (void)
{
#ifdef FAIR
  unfair = 0;
  mt.it_interval.tv_sec = 0;
  mt.it_interval.tv_usec = 0;
  mt.it_value.tv_sec = SLICE_SEC;
  mt.it_value.tv_usec = SLICE_USEC;
  signal (SIGVTALRM, interrupt_handler);
  setitimer (ITIMER_VIRTUAL, &mt, NULL);
#endif
}


/*------------------------------------------------------------------------
 *
 * Enable timer interrupts: must be called with interrupts disabled.
 *
 *------------------------------------------------------------------------
 */
#ifdef FAIR
void context_enable (void)
{
  LEAVE_CS (current_process);
}
#endif


/*------------------------------------------------------------------------
 *
 * Disable timer interrupts: must be called with interrupts enabled.
 *
 *------------------------------------------------------------------------
 */
#ifdef FAIR
void context_disable (void)
{
  ENTER_CS (current_process);
}
#endif

 
/*------------------------------------------------------------------------
 *
 * Called with interrupts disabled. Enables interrupts on termination.
 *
 *------------------------------------------------------------------------
 */
#ifdef __ia64__
void context_switch (context_t *p)
#else
void context_switch (process_t *p)
#endif
{
#if defined(__linux__) && defined(__ia64__)
  old_process = current_process;
  if (current_process) {
     current_process = p;
     swapcontext(&(old_process->uc), &(p->uc));
  }
  else {
     current_process = p;
     swapcontext(&dummy, &(p->uc));
  }
#else
  if (!current_process || !_setjmp (current_process->c.buf)) {
    current_process = p;
    _longjmp (p->c.buf,1);
  }
#endif
  if (terminated_process) {
    context_destroy (terminated_process);
    terminated_process = NULL;
  }
  context_enable ();
}

/*------------------------------------------------------------------------
 *
 *  Called with interrupts disabled
 *  cleaup routine
 *
 *------------------------------------------------------------------------
 */
void context_cleanup (void)
{
  if (terminated_process) {
    context_destroy (terminated_process);
    terminated_process = NULL;
  }
}

/*------------------------------------------------------------------------
 *
 * Called with interrupts disabled;
 * Enables interrupts on exit.
 *
 *------------------------------------------------------------------------
 */
static void context_stub (void)
{
#ifdef FAIR
  context_internal_init ();
#endif
  if (terminated_process) {
    context_destroy (terminated_process);
    terminated_process = NULL;
  }
  context_enable ();
#ifdef __ia64__
  (*current_process->start)();
#else
  (*current_process->c.start)();
#endif
  context_disable ();
  terminated_process = current_process;
  /*
   * It would be nice to delete the process here, but we can't do that
   * because we're deleting the stack we're executing on! Some other
   * process must free our stack.
   */
  context_switch (context_select ());
}


/*------------------------------------------------------------------------
 *
 *  Called on exit; must be called with interrupts disabled
 *
 *------------------------------------------------------------------------
 */
void context_exit (void)
{
  terminated_process = current_process;
  context_switch (context_select ());
}

/*
 * Non-portable code is in this function
 */
#ifdef __ia64__
void context_init (context_t* c, void (*f)(void))
#else
void context_init (process_t* p, void (*f)(void))
#endif
{
  void *stack;
  int n;

#ifdef __ia64__
  c->start = f;
  stack = c->stack;
  n = c->sz;
#else
  p->c.start = f;
  stack = p->c.stack;
  n = p->c.sz;
#endif

#if defined(__linux__) && defined(__ia64__)
  getcontext(&(c->uc));
#else
  _setjmp (p->c.buf);
#endif

#ifdef FAIR
#ifdef __ia64__
  c->in_cs = 0;
  c->pending = 0;
  c->interrupted = 0;
#else
  p->c.in_cs = 0;
  p->c.pending = 0;
  p->c.interrupted = 0;
#endif
#endif

#if defined(__sparc__) && !defined(__svr4__)

#define INIT_SP(p) (int)((double*)(p)->c.stack + (p)->c.sz/sizeof(double)-11)
#define CURR_SP(p) (p)->c.buf[2]

  /* Need 12 more doubles to save register windows. */
  p->c.buf[3] = p->c.buf[4] = (int)context_stub;
  p->c.buf[2] = (int)((double*)stack + n/sizeof(double)-11);

#elif defined(__sparc__)

#define INIT_SP(p) (int)((double*)(p)->c.stack + (p)->c.sz/sizeof(double)-11)
#define CURR_SP(p) (p)->c.buf[1]

  /* Need 12 more doubles to save register windows. */
  p->c.buf[2] = (int)context_stub;
  p->c.buf[1] = (int)((double*)stack + n/sizeof(double)-11);

#elif defined(__NetBSD__) && defined(__i386__)

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz-4)
#define CURR_SP(p) (p)->c.buf[2]

  p->c.buf[0] = (int)context_stub;
  p->c.buf[2] = (int)((char*)stack+n-4);

#elif defined(__NetBSD__) && defined(__m68k__)

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[2]

  p->c.buf[5] = (int)context_stub;
  p->c.buf[2] = (int)((char*)stack+n-4);

#elif defined(__linux__) && defined(__i386__)

/* This works with RedHat 7.1 */
#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[0].__jmpbuf[4]

  p->c.buf[0].__jmpbuf[5] = (int)context_stub;
  p->c.buf[0].__jmpbuf[4] = (int)((char*)stack+n-4);


/* 
   This used to work with Linux, version unknown... 
   AFAIK there is no way to distinguish between the lines
   below and above using ifdefs... argh!

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[0].__sp

  p->c.buf[0].__pc = (__ptr_t)context_stub;
  p->c.buf[0].__sp = (__ptr_t)((char*)stack+n-4);
*/

#elif defined(__FreeBSD__) && defined(__i386__)

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[0]._jb[2]

  p->c.buf[0]._jb[0] = (long)context_stub;
  p->c.buf[0]._jb[2] = (long)((char*)(stack+n-4));

#elif defined (__alpha)

#define INIT_SP(p) (long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[34]

  /* entry point needs adjustment */
  p->c.buf[30] = (long)context_stub+8;
  p->c.buf[34] = (long)stack+n-8;

#elif defined (mips) && defined(__sgi)

#define INIT_SP(p) (long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[JB_SP]

  p->c.buf[JB_PC] = (long)context_stub;
  p->c.buf[JB_SP] = (long)stack+n-8;

#elif defined(__hppa) && defined (__hpux)

#define INIT_SP(p) (long)((char*)(p)->c.stack)
#define CURR_SP(p) ((unsigned long*)&(p)->c.buf)[1]

  /* two stack frames, stack grows up */
  ((unsigned long*)&p->c.buf)[1] = (unsigned long)stack + 128; 

  /* function pointers point to a deref table */
  ((unsigned long*)&p->c.buf)[44] = 
    *((unsigned long*)((unsigned long)context_stub & ~3));

#elif defined(PC_OFFSET) && defined(SP_OFFSET)

#define Max(a,b) ((a) > (b) ? (a) : (b))

#define INIT_SP(p) (unsigned long)((unsigned long)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) ((unsigned long*)&p->c.buf)[SP_OFFSET]

  ((unsigned long*)&p->c.buf)[PC_OFFSET] = (unsigned long)context_stub;
#if !defined (STACK_DIR_UP)
  ((unsigned long*)&p->c.buf)[SP_OFFSET] = (unsigned long)stack+n-Max(sizeof(long),sizeof(double));
#else
  ((unsigned long*)&p->c.buf)[SP_OFFSET] = (unsigned long)stack+Max(sizeof(long),sizeof(double));
#endif

#elif defined(__linux__) && defined(__ia64__)
#define INIT_SP(p) (long)((char*)(p)->stack + (p)->sz)
#define CURR_SP(p) GetSP()

  c->uc.uc_link = 0;
  c->uc.uc_stack.ss_sp = stack;
  c->uc.uc_stack.ss_size = n;
  c->uc.uc_stack.ss_flags = 0;
  makecontext(&(c->uc), context_stub, 0);

#else
#error Unknown machine/OS combination
#endif
}



/*------------------------------------------------------------------------
 *
 *  Save context to file
 *
 *------------------------------------------------------------------------
 */
#ifdef __ia64__
void context_write (FILE *fp, context_t *p)
#else
void context_write (FILE *fp, process_t *p)
#endif
{
  int i;
  int n;
  unsigned long *l;
  unsigned char *curr_sp, *init_sp;

#ifdef __ia64__
  l = (unsigned long *)&p->buf;
  for (i=0; i < sizeof(p->buf)/sizeof(unsigned long); i++) {
    fprintf (fp, "%lu\n", *l);
    l++;
  }
#else
  l = (unsigned long *)&p->c.buf;
  for (i=0; i < sizeof(p->c.buf)/sizeof(unsigned long); i++) {
    fprintf (fp, "%lu\n", *l);
    l++;
  }
#endif
 /* save stack */
  init_sp = (unsigned char *)INIT_SP(p);
  curr_sp = (unsigned char *)CURR_SP(p);

  if (init_sp > curr_sp) {
    curr_sp = init_sp;
    init_sp = (unsigned char *)CURR_SP(p);
  }
  /*
   * This is broken. if init_sp == curr_sp, do we save or not???
   * (machine-dependent)
   *
   */
  while (init_sp < curr_sp) {
    i = *init_sp;
    fprintf (fp, "%d\n", i);
    init_sp++;
  }
#ifdef FAIR
#ifdef __ia64__
  fprintf (fp, "%d\n%d\n%d\n", p->in_cs, p->pending, p->interrupted);
#else
  fprintf (fp, "%d\n%d\n%d\n", p->c.in_cs, p->c.pending, p->c.interrupted);
#endif
#endif
}


/*------------------------------------------------------------------------
 *
 *  Read Context
 *
 *------------------------------------------------------------------------
 */
#ifdef __ia64__
void context_read (FILE *fp, context_t *p)
#else
void context_read (FILE *fp, process_t *p)
#endif
{
  int i;
  int n;
  unsigned long *l;
  unsigned char *init_sp, *curr_sp;

#ifdef __ia64__
  l = (unsigned long *)&p->buf;
  for (i=0; i < sizeof(p->buf)/sizeof(unsigned long); i++) {
    fscanf (fp, "%lu", l++);
  }
#else
  l = (unsigned long *)&p->c.buf;
  for (i=0; i < sizeof(p->c.buf)/sizeof(unsigned long); i++) {
    fscanf (fp, "%lu", l++);
  }
#endif
  init_sp = (unsigned char *)INIT_SP(p);
  curr_sp = (unsigned char *)CURR_SP(p);

  if (init_sp > curr_sp) {
    curr_sp = init_sp;
    init_sp = (unsigned char *)CURR_SP(p);
  }
  /* this is broken too... */
  while (init_sp < curr_sp) {
    fscanf (fp, "%d", &i);
    *init_sp = i;
    init_sp++;
  }
#ifdef FAIR
#ifdef __ia64__
  fscanf (fp, "%d%d%d\n", &p->in_cs, &p->pending, &p->interrupted);
#else
  fscanf (fp, "%d%d%d\n", &p->c.in_cs, &p->c.pending, &p->c.interrupted);
#endif
#endif
}
