
#include <stdio.h>
#include <pthread.h>

#define THREADS 2
#define SUM_TO 1000000LLU

// to eliminate parallelism, start with
//   $> taskset 1 ./program


static volatile unsigned long long res = 0;

void*
sum_unguarded (void *args)
{
  int id = *((int*)args);

  unsigned long i;
  for (i = id; i <= SUM_TO; i += THREADS)
    {
      /* enter critical section *********************************************/
      // no-op
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      // no-op
      /**********************************************************************/
    }

  return NULL;
}

void*
sum_turns (void *args)
{
  int id = *((int*)args);

  static volatile int turn = 0;

  unsigned long i;
  for (i = id; i <= SUM_TO; i += THREADS)
    {
      /* enter critical section *********************************************/
      while (turn != id);
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      turn = id ^ 1;
      /**********************************************************************/
    }

  return NULL;
}

void*
sum_flags (void *args)
{
  int id = *((int*)args);

  static volatile int flags[2] = { 0 };

  unsigned long i;
  for (i = id; i <= SUM_TO; i += 2)
    {
      /* enter critical section *********************************************/
      flags[id] = 1;
      while (flags[id ^ 1] == 1);
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      flags[id] = 0;
      /**********************************************************************/
    }

  return NULL;
}

void*
sum_peterson (void *args)
{
  int id = *((int*)args);

  static volatile int flags[2] = { 0 };
  static volatile int turn = 0;

  unsigned long i;
  for (i = id; i <= SUM_TO; i += 2)
    {
      /* enter critical section *********************************************/
      flags[id] = 1; turn = id ^ 1;
      while ((flags[id ^ 1] == 1) && turn == (id ^ 1));
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      flags[id] = 0;
      /**********************************************************************/
    }

  return NULL;
}

void*
sum_dekker (void *args)
{
  int id = *((int*)args);

  static volatile int flags[2] = { 0 };
  static volatile int turn = 0;

  unsigned long i;
  for (i = id; i <= SUM_TO; i += 2)
    {
      /* enter critical section *********************************************/
      flags[id] = 1;
      while (flags[id ^ 1] == 1)
        if (turn == (id ^ 1))
          {
            flags[id] = 0;
            while (turn == (id ^ 1));
            flags[id] = 1;
          }
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      turn = id ^ 1;
      flags[id] = 0;
      /**********************************************************************/
    }

  return NULL;
}

static int
max (volatile long long int *v, size_t n)
{
  size_t i;
  int res = 0;
  for (i = 0; i < n; ++i)
    if (v[i] > res)
      res = v[i];
  return res;
}

void*
sum_bakery (void *args)
{
  int id = *((int*)args);

  static volatile int choosing[THREADS] = { 0 };
  static volatile long long int num[THREADS] = { 0 };

  unsigned long i;
  for (i = id; i <= SUM_TO; i += THREADS)
    {
      /* enter critical section *********************************************/
      choosing[id] = 1;
      num[id] = max(num, THREADS) + 1;
      choosing[id] = 0;
      int j;
      for (j = 0; j < THREADS; ++j)
        {
          while (choosing[j] == 1);
          while ((num[j] != 0) && (num[j] < num[id] || (num[j] == num[id] && j < id)));
        }
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      num[id] = 0;
      /**********************************************************************/
    }

  return NULL;
}

void*
sum_test_and_set (void *args)
{
  int id = *((int*)args);

  static int lock = 0;

  unsigned long i;
  for (i = id; i <= SUM_TO; i += THREADS)
    {
      /* enter critical section *********************************************/
      while (__sync_lock_test_and_set(&lock, 1)) {
        while (lock);
      }
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      __sync_lock_release(&lock);
      /**********************************************************************/
    }

  return NULL;
}

void*
sum_semaphore (void *args)
{
  int id = *((int*)args);

  static pthread_mutex_t semaphore = PTHREAD_MUTEX_INITIALIZER;

  unsigned long i;
  for (i = id; i <= SUM_TO; i += THREADS)
    {
      /* enter critical section *********************************************/
      pthread_mutex_lock(&semaphore);
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      pthread_mutex_unlock(&semaphore);
      /**********************************************************************/
    }

  return NULL;

}

void*
sum_custom (void *args)
{
  int id = *((int*)args);

  unsigned long i;
  for (i = id; i <= SUM_TO; i += THREADS)
    {
      /* enter critical section *********************************************/
      // TODO!
      /**********************************************************************/

      res += i;

      /* leave critical section *********************************************/
      // TODO!
      /**********************************************************************/
    }

  return NULL;
}

typedef void*(*pthread_func_t)(void*);

struct guard_type_t
{
  pthread_func_t func;
  const char *name;
  size_t max_threads;
};

static const struct guard_type_t guards[] = {
#if defined(HAVE_UNGUARDED)
  { sum_unguarded, "unguarded", 0 },
#endif
#if defined(HAVE_TURNS)
  { sum_turns, "take turns", 2 },
#endif
#if defined(HAVE_FLAGS)
  { sum_flags, "raise flags", 2 },
#endif
#if defined(HAVE_PETERSON)
  { sum_peterson, "Peterson's Algorithm", 2 },
#endif
#if defined(HAVE_DEKKER)
  { sum_dekker, "Dekker's Algorithm", 2 },
#endif
#if defined(HAVE_BAKERY)
  { sum_bakery, "Bakery Algorithm (Lamport)", 0 },
#endif
#if defined(HAVE_TEST_AND_SET)
  { sum_test_and_set, "test&set", 0 },
#endif
#if defined(HAVE_SEMAPHORE)
  { sum_semaphore, "semaphore", 0 },
#endif
#if defined(HAVE_CUSTOM)
  { sum_custom, "custom", 2 },
#endif
  { NULL, NULL, 0 }
};

int
main (void)
{
  pthread_t threads[THREADS] = { 0 };
  int args[THREADS] = { 0 };

  const struct guard_type_t *guard = guards;
  while (guard->func != NULL)
    {
      res = 0;

      size_t nthreads = (guard->max_threads > 0 && guard->max_threads < THREADS) ? guard->max_threads : THREADS;
      printf("starting experiment \"%s\" with %zu threads\n", guard->name, nthreads);

      size_t i;
      for (i = 0; i < nthreads; ++i)
        {
          args[i] = i;
          if (pthread_create(threads + i, NULL, guard->func, args + i) != 0)
            {
              perror("pthread_create");
              return 1;
            }
        }

      for (i = 0; i < nthreads; ++i)
        if (pthread_join(threads[i], NULL) != 0)
          {
            perror("pthread_join");
            return 1;
          }

      printf("sum is:        %20llu\n", res);
      printf("sum should be: %20llu\n", (SUM_TO * (SUM_TO + 1)) / 2);

      guard++;
    }

  return 0;
}
