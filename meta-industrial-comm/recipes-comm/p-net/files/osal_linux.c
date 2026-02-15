/* osal_linux.c — Linux/POSIX implementation of the rt-labs OSAL for p-net */
#define _GNU_SOURCE
#include "osal.h"
#include "osal_log.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

/* ── logging ──────────────────────────────────────────────────────────────── */

static void default_log (uint8_t type, const char * fmt, ...)
{
   va_list ap;
   va_start (ap, fmt);
   vfprintf (stderr, fmt, ap);
   va_end (ap);
}
os_log_t os_log = default_log;

/* ── memory ───────────────────────────────────────────────────────────────── */

void * os_malloc (size_t size) { return malloc (size); }
void   os_free   (void * ptr)  { free (ptr); }

/* ── time ─────────────────────────────────────────────────────────────────── */

void os_usleep (uint32_t usec)
{
   struct timespec ts = { (time_t)(usec / 1000000), (long)(usec % 1000000) * 1000L };
   nanosleep (&ts, NULL);
}

uint64_t os_get_current_time_us (void)
{
   struct timespec ts;
   clock_gettime (CLOCK_MONOTONIC, &ts);
   return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

os_tick_t os_tick_current (void)   { return os_get_current_time_us (); }
os_tick_t os_tick_from_us (uint32_t usec) { return (os_tick_t)usec; }
void      os_tick_sleep   (os_tick_t ticks) { os_usleep ((uint32_t)ticks); }

/* ── thread ───────────────────────────────────────────────────────────────── */

struct os_thread
{
   pthread_t tid;
};

typedef struct
{
   void (*entry) (void *);
   void * arg;
} thread_trampoline_t;

static void * thread_entry (void * arg)
{
   thread_trampoline_t t = *(thread_trampoline_t *)arg;
   free (arg);
   t.entry (t.arg);
   return NULL;
}

os_thread_t * os_thread_create (
   const char * name,
   uint32_t     priority,
   size_t       stacksize,
   void (*entry) (void *),
   void *       arg)
{
   (void)priority;
   os_thread_t * t = malloc (sizeof (*t));
   if (!t) return NULL;

   thread_trampoline_t * tr = malloc (sizeof (*tr));
   if (!tr) { free (t); return NULL; }
   tr->entry = entry;
   tr->arg   = arg;

   pthread_attr_t attr;
   pthread_attr_init (&attr);
   if (stacksize > 0)
      pthread_attr_setstacksize (&attr, stacksize < 65536 ? 65536 : stacksize);

   if (pthread_create (&t->tid, &attr, thread_entry, tr) != 0)
   {
      pthread_attr_destroy (&attr);
      free (tr);
      free (t);
      return NULL;
   }
   pthread_attr_destroy (&attr);
   pthread_setname_np (t->tid, name);
   return t;
}

/* ── mutex ────────────────────────────────────────────────────────────────── */

struct os_mutex
{
   pthread_mutex_t mtx;
};

os_mutex_t * os_mutex_create (void)
{
   os_mutex_t * m = malloc (sizeof (*m));
   if (!m) return NULL;
   pthread_mutex_init (&m->mtx, NULL);
   return m;
}
void os_mutex_lock    (os_mutex_t * m) { pthread_mutex_lock (&m->mtx); }
void os_mutex_unlock  (os_mutex_t * m) { pthread_mutex_unlock (&m->mtx); }
void os_mutex_destroy (os_mutex_t * m) { pthread_mutex_destroy (&m->mtx); free (m); }

/* ── semaphore ────────────────────────────────────────────────────────────── */

struct os_sem
{
   pthread_mutex_t mtx;
   pthread_cond_t  cnd;
   size_t          count;
};

os_sem_t * os_sem_create (size_t count)
{
   os_sem_t * s = malloc (sizeof (*s));
   if (!s) return NULL;
   pthread_mutex_init (&s->mtx, NULL);
   pthread_cond_init  (&s->cnd, NULL);
   s->count = count;
   return s;
}

bool os_sem_wait (os_sem_t * s, uint32_t time_in_us)
{
   struct timespec abs;
   clock_gettime (CLOCK_REALTIME, &abs);
   abs.tv_sec  += (time_t)(time_in_us / 1000000);
   abs.tv_nsec += (long)((time_in_us % 1000000) * 1000L);
   if (abs.tv_nsec >= 1000000000L) { abs.tv_nsec -= 1000000000L; abs.tv_sec++; }

   pthread_mutex_lock (&s->mtx);
   while (s->count == 0)
   {
      if (time_in_us == UINT32_MAX)
      {
         pthread_cond_wait (&s->cnd, &s->mtx);
      }
      else
      {
         int rc = pthread_cond_timedwait (&s->cnd, &s->mtx, &abs);
         if (rc == ETIMEDOUT) { pthread_mutex_unlock (&s->mtx); return false; }
      }
   }
   s->count--;
   pthread_mutex_unlock (&s->mtx);
   return true;
}

void os_sem_signal (os_sem_t * s)
{
   pthread_mutex_lock (&s->mtx);
   s->count++;
   pthread_cond_signal (&s->cnd);
   pthread_mutex_unlock (&s->mtx);
}

void os_sem_destroy (os_sem_t * s)
{
   pthread_cond_destroy  (&s->cnd);
   pthread_mutex_destroy (&s->mtx);
   free (s);
}

/* ── event flags ──────────────────────────────────────────────────────────── */

struct os_event
{
   pthread_mutex_t mtx;
   pthread_cond_t  cnd;
   uint32_t        flags;
};

os_event_t * os_event_create (void)
{
   os_event_t * e = malloc (sizeof (*e));
   if (!e) return NULL;
   pthread_mutex_init (&e->mtx, NULL);
   pthread_cond_init  (&e->cnd, NULL);
   e->flags = 0;
   return e;
}

bool os_event_wait (os_event_t * e, uint32_t mask, uint32_t * value, uint32_t time_in_us)
{
   struct timespec abs;
   clock_gettime (CLOCK_REALTIME, &abs);
   abs.tv_sec  += (time_t)(time_in_us / 1000000);
   abs.tv_nsec += (long)((time_in_us % 1000000) * 1000L);
   if (abs.tv_nsec >= 1000000000L) { abs.tv_nsec -= 1000000000L; abs.tv_sec++; }

   pthread_mutex_lock (&e->mtx);
   while ((e->flags & mask) == 0)
   {
      if (time_in_us == UINT32_MAX)
      {
         pthread_cond_wait (&e->cnd, &e->mtx);
      }
      else
      {
         int rc = pthread_cond_timedwait (&e->cnd, &e->mtx, &abs);
         if (rc == ETIMEDOUT)
         {
            *value = e->flags & mask;
            pthread_mutex_unlock (&e->mtx);
            return false;
         }
      }
   }
   *value = e->flags & mask;
   pthread_mutex_unlock (&e->mtx);
   return true;
}

void os_event_set (os_event_t * e, uint32_t value)
{
   pthread_mutex_lock (&e->mtx);
   e->flags |= value;
   pthread_cond_broadcast (&e->cnd);
   pthread_mutex_unlock (&e->mtx);
}

void os_event_clr (os_event_t * e, uint32_t value)
{
   pthread_mutex_lock (&e->mtx);
   e->flags &= ~value;
   pthread_mutex_unlock (&e->mtx);
}

void os_event_destroy (os_event_t * e)
{
   pthread_cond_destroy  (&e->cnd);
   pthread_mutex_destroy (&e->mtx);
   free (e);
}

/* ── mailbox ──────────────────────────────────────────────────────────────── */

struct os_mbox
{
   pthread_mutex_t mtx;
   pthread_cond_t  not_empty;
   pthread_cond_t  not_full;
   size_t          capacity;
   size_t          head;
   size_t          tail;
   size_t          count;
   void **         buf;
};

os_mbox_t * os_mbox_create (size_t size)
{
   os_mbox_t * mb = malloc (sizeof (*mb));
   if (!mb) return NULL;
   mb->buf = malloc (size * sizeof (void *));
   if (!mb->buf) { free (mb); return NULL; }
   pthread_mutex_init (&mb->mtx, NULL);
   pthread_cond_init  (&mb->not_empty, NULL);
   pthread_cond_init  (&mb->not_full, NULL);
   mb->capacity = size;
   mb->head = mb->tail = mb->count = 0;
   return mb;
}

bool os_mbox_post (os_mbox_t * mb, void * msg, uint32_t time_in_us)
{
   struct timespec abs;
   clock_gettime (CLOCK_REALTIME, &abs);
   abs.tv_sec  += (time_t)(time_in_us / 1000000);
   abs.tv_nsec += (long)((time_in_us % 1000000) * 1000L);
   if (abs.tv_nsec >= 1000000000L) { abs.tv_nsec -= 1000000000L; abs.tv_sec++; }

   pthread_mutex_lock (&mb->mtx);
   while (mb->count >= mb->capacity)
   {
      if (time_in_us == UINT32_MAX)
      {
         pthread_cond_wait (&mb->not_full, &mb->mtx);
      }
      else
      {
         if (pthread_cond_timedwait (&mb->not_full, &mb->mtx, &abs) == ETIMEDOUT)
         { pthread_mutex_unlock (&mb->mtx); return false; }
      }
   }
   mb->buf[mb->tail] = msg;
   mb->tail = (mb->tail + 1) % mb->capacity;
   mb->count++;
   pthread_cond_signal (&mb->not_empty);
   pthread_mutex_unlock (&mb->mtx);
   return true;
}

bool os_mbox_fetch (os_mbox_t * mb, void ** msg, uint32_t time_in_us)
{
   struct timespec abs;
   clock_gettime (CLOCK_REALTIME, &abs);
   abs.tv_sec  += (time_t)(time_in_us / 1000000);
   abs.tv_nsec += (long)((time_in_us % 1000000) * 1000L);
   if (abs.tv_nsec >= 1000000000L) { abs.tv_nsec -= 1000000000L; abs.tv_sec++; }

   pthread_mutex_lock (&mb->mtx);
   while (mb->count == 0)
   {
      if (time_in_us == UINT32_MAX)
      {
         pthread_cond_wait (&mb->not_empty, &mb->mtx);
      }
      else
      {
         if (pthread_cond_timedwait (&mb->not_empty, &mb->mtx, &abs) == ETIMEDOUT)
         { pthread_mutex_unlock (&mb->mtx); return false; }
      }
   }
   *msg = mb->buf[mb->head];
   mb->head = (mb->head + 1) % mb->capacity;
   mb->count--;
   pthread_cond_signal (&mb->not_full);
   pthread_mutex_unlock (&mb->mtx);
   return true;
}

void os_mbox_destroy (os_mbox_t * mb)
{
   pthread_cond_destroy  (&mb->not_empty);
   pthread_cond_destroy  (&mb->not_full);
   pthread_mutex_destroy (&mb->mtx);
   free (mb->buf);
   free (mb);
}

/* ── timer ────────────────────────────────────────────────────────────────── */

struct os_timer
{
   timer_t              tid;
   void (*fn) (os_timer_t *, void *);
   void *               arg;
   uint32_t             us;
   bool                 oneshot;
};

static void timer_handler (union sigval sv)
{
   os_timer_t * t = sv.sival_ptr;
   if (t->fn) t->fn (t, t->arg);
}

os_timer_t * os_timer_create (uint32_t us, void (*fn) (os_timer_t *, void *), void * arg, bool oneshot)
{
   os_timer_t * t = malloc (sizeof (*t));
   if (!t) return NULL;
   t->fn      = fn;
   t->arg     = arg;
   t->us      = us;
   t->oneshot = oneshot;

   struct sigevent sev = {0};
   sev.sigev_notify            = SIGEV_THREAD;
   sev.sigev_notify_function   = timer_handler;
   sev.sigev_value.sival_ptr   = t;

   if (timer_create (CLOCK_MONOTONIC, &sev, &t->tid) != 0)
   { free (t); return NULL; }
   return t;
}

void os_timer_set (os_timer_t * t, uint32_t us) { t->us = us; }

void os_timer_start (os_timer_t * t)
{
   struct itimerspec its = {0};
   its.it_value.tv_sec  = t->us / 1000000;
   its.it_value.tv_nsec = (long)(t->us % 1000000) * 1000L;
   if (!t->oneshot)
   {
      its.it_interval = its.it_value;
   }
   timer_settime (t->tid, 0, &its, NULL);
}

void os_timer_stop (os_timer_t * t)
{
   struct itimerspec its = {0};
   timer_settime (t->tid, 0, &its, NULL);
}

void os_timer_destroy (os_timer_t * t)
{
   os_timer_stop (t);
   timer_delete (t->tid);
   free (t);
}
