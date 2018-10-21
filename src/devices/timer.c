#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);
static void wake_up (void);
static bool compare (struct list_elem *e1, struct list_elem *e2, void* AUX);
/* Structure to store thread properties in list of sleeping
   threads. */
static struct sleeper_struct
{ 
  int *priority;
  struct list_elem *thread_elem;
  int64_t ticks;
  struct list_elem elem;
};

/* List of sleeper threads */
static struct list sleeper_list;

/* Semaphore to ensure list insert is done for one thread 
   at any point of time. */
static struct semaphore sema;

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
  //Initializing sleeper_list
  list_init(&sleeper_list);
  sema_init(&sema,1);
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  //printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (loops_per_tick | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Function to return thread with least number of ticks or greatest 
   priority. */
bool
compare (struct list_elem *e1, struct list_elem *e2, void *aux UNUSED)
{
  struct sleeper_struct *s1 = list_entry(e1, struct sleeper_struct, elem); 
  struct sleeper_struct *s2 = list_entry(e2, struct sleeper_struct, elem);
  return (s1->ticks < s2->ticks || *(s1->priority) > *(s2->priority));
    
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks)
{
  int64_t start = timer_ticks ();
  enum intr_level old_level;
  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
  {
    struct sleeper_struct *sleeper = malloc(sizeof(struct sleeper_struct));
    int *priority;
    
    struct list_elem *thread_elm = &thread_current()->elem;
    priority = &thread_current()->priority;
    
    /* Use elem, time to wake up and priority to define a sleeping thread. */
    sleeper->thread_elem = thread_elm;
    sleeper->ticks = ticks + start;
    sleeper->priority = priority;

    /* Synchronizing access to sleeper_list. */
    sema_down(&sema);
    list_insert_ordered (&sleeper_list, &sleeper->elem, &compare, NULL);   
    sema_up(&sema);
   
    /* Disabling interrupts for thread_block only. */
    old_level = intr_disable();   
    thread_block();
    intr_set_level(old_level);  
  }
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Wake up function that wakes up threads in 
 * sleeper list. */
void
wake_up ()
{
  if (!list_empty(&sleeper_list))
  { 
    struct sleeper_struct *first_sleeper = malloc(sizeof(struct sleeper_struct));
    first_sleeper = list_entry (list_begin(&sleeper_list), struct sleeper_struct, elem);
    
    /* Pop from sleeper_list if sufficient ticks have elapsed. */
    if (ticks >= first_sleeper->ticks)
    {
    
      /* Unblock from top of sleeper list. */
      struct sleeper_struct *sleeper = list_entry(list_pop_front(&sleeper_list),struct sleeper_struct, elem);
      thread_unblock(list_entry(sleeper->thread_elem, struct thread, elem));
      
      /* Recursively wake up until the `ticks` condition fails. */ 
      wake_up();
    }
  }
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;
  /* If MLFQS is active, update params accordingly. */
  if (thread_mlfqs)
  { 
    /* Update load_avg and recent_cpu every second. */
    if (ticks % (TIMER_FREQ) == 0)
    {
      update_load_avg ();
      update_recent_cpu ();
    }
    /* Every 4th tick, update priorities. */
    if (ticks % 4 == 0)
      update_priorities ();
  }
  /* Wake up sleeping threads if necessary. */
  wake_up();
  thread_tick ();
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
