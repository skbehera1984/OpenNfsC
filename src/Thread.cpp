/*
   Copyright (C) 2020 by Sarat Kumar Behera <beherasaratkumar@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#define _WIN32_WINNT 0x400

#include <stdTypes.h>
#include <Thread.h>
#include <iostream>
#include <stdio.h>
#include <fcntl.h>

#ifdef R_WIN
#include <windows.h>
#include <process.h>
#elif defined(R_UNIX)
#ifdef R_OS_VXWORKS
  #include <sys/times.h>
#else
  #include <sys/time.h>
#endif
  #include <unistd.h>
  #include <errno.h>
#endif

/* don't use PTHREAD_STACK_MIN since it's already defined too small */
#define THREAD_STACK_MIN (8192*sizeof(long))

#ifdef R_NAMESPACE_SUPPORTED
using namespace OpenNfsC::Thread;
using namespace std;
#endif


#ifdef R_WIN

#define ETIMEDOUT ((int)(WAIT_TIMEOUT))

// pthread_condattr_t is not required by ConditionVar. However a place
// holder definition has been supplied so that the pthread condition
// variable function calls implemented below have the same signature as their Unix counterparts.
typedef int pthread_condattr_t;

// Required to give our windows gettimeofday() implementation
// the same signature as it's Unix counterpart.
struct timezone
{
  int tz_minuteswest;
  int tz_dsttime;
};


static void GetUnixEpochAsSystemTime(SYSTEMTIME &t)
{
  t.wYear = 1970;
  t.wMonth = 1; // Jan
  t.wDayOfWeek = 4; // Thursday
  t.wDay = 1;
  t.wHour = 0;
  t.wMinute = 0;
  t.wSecond = 0;
  t.wMilliseconds = 0;
}

static void GetUnixEpochAsFileTime(FILETIME &t)
{
  SYSTEMTIME epoch;
  GetUnixEpochAsSystemTime(epoch);
  SystemTimeToFileTime(&epoch, &t);
}

static int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  SYSTEMTIME sysTimeNow;
  GetLocalTime(&sysTimeNow);

  FILETIME fileTimeNow;
  SystemTimeToFileTime(&sysTimeNow, &fileTimeNow);

  ULONGLONG now = 10 * ((ULARGE_INTEGER&)fileTimeNow).QuadPart;

  FILETIME fileTimeEpoch;
  GetUnixEpochAsFileTime(fileTimeEpoch);

  ULONGLONG epoch = 10 * ((ULARGE_INTEGER&)fileTimeEpoch).QuadPart;

  now -= epoch;

  tv->tv_sec = now / 1000000;
  tv->tv_usec = now % 1000000;

  return 0;
}

static int pthread_cond_init(pthread_cond_t *cv, const pthread_condattr_t *)
{
  cv->waiters_count = 0;
  cv->was_broadcast = 0;
  cv->sema = CreateSemaphore(NULL,       // no security
                             0,          // initially 0
                             0x7fffffff, // max count
                             NULL);      // unnamed
  InitializeCriticalSection(&cv->waiters_count_lock);
  cv->waiters_done = CreateEvent(NULL,  // no security
                                 FALSE, // auto-reset
                                 FALSE, // non-signaled initially
                                 NULL); // unnamed
  return 0;
}

static int pthread_cond_destroy(pthread_cond_t *cv)
{
  CloseHandle(cv->sema);
  DeleteCriticalSection(&cv->waiters_count_lock);
  CloseHandle(cv->waiters_done);
  return 0;
}

/**
   The pthread_cond_wait function uses two steps:

   1. It waits for the sema_ semaphore to be signaled, which indicates
      that a pthread_cond_broadcast or pthread_cond_signal has occurred.

   2. It sets the waiters_done_ auto-reset event into the signaled state
      when the last waiting thread is about to leave the
      pthread_cond_wait critical section.

   Step 2 collaborates with the pthread_cond_broadcast function
   described below to ensure fairness. But first, here's the
   implementation of pthread_cond_wait:
**/
static int pthread_cond_timedwait(pthread_cond_t *cv,
                                  pthread_mutex_t *external_mutex,
                                  const struct timespec *abstime)
{
  int ret = 0;

  // Avoid race conditions
  EnterCriticalSection (&cv->waiters_count_lock);
  cv->waiters_count++;
  LeaveCriticalSection (&cv->waiters_count_lock);

  DWORD timeout = INFINITE;
  if (abstime)
  {
    struct timeval now;
    gettimeofday(&now, NULL);
    timeout = (abstime->tv_sec - now.tv_sec) * 1000;
    timeout += ((abstime->tv_nsec / 1000) - now.tv_usec) / 1000;
    if (timeout < 0)
    {
      timeout = 0; // don't wait
    }
  }

  // This call atomically releases the mutex and waits on the
  // semaphore until <pthread_cond_signal> or <pthread_cond_broadcast>
  // are called by another thread.
  DWORD code = SignalObjectAndWait(*external_mutex,
                                   cv->sema,
                                   timeout,
                                   FALSE);
  if (code != WAIT_OBJECT_0)
  {
    switch (code)
    {
      case WAIT_TIMEOUT:
        ret = ETIMEDOUT;
      break;
      case WAIT_IO_COMPLETION:
        ret = EINTR;
      break;
      default:
        ret = EINVAL;
      break;
    }
  }

  // Reacquire lock to avoid race conditions.
  EnterCriticalSection (&cv->waiters_count_lock);

  // We're no longer waiting...
  cv->waiters_count--;

  // Check to see if we're the last waiter after <pthread_cond_broadcast>.
  int last_waiter = cv->was_broadcast && (cv->waiters_count == 0);

  LeaveCriticalSection (&cv->waiters_count_lock);

  //If we're the last waiter thread during this particular broadcast then let all the other threads proceed.
  if (last_waiter)
    // This call atomically signals the <waiters_done> event and waits until
    // it can acquire the <external_mutex>.  This is required to ensure fairness.
    SignalObjectAndWait (cv->waiters_done, *external_mutex, INFINITE, FALSE);
  else
    // Always regain the external mutex since that's the guarantee we give to our callers.
    WaitForSingleObject(*external_mutex, INFINITE);

  return ret;
}

static int pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *external_mutex)
{
  return pthread_cond_timedwait(cv, external_mutex, NULL);
}

/**
   The pthread_cond_signal function is straightforward since it just
   releases one waiting thread. Therefore, it simply increments the
   sema_ semaphore by 1:
**/
static int pthread_cond_signal (pthread_cond_t *cv)
{
  EnterCriticalSection (&cv->waiters_count_lock);
  int have_waiters = cv->waiters_count > 0;
  LeaveCriticalSection (&cv->waiters_count_lock);

  // If there aren't any waiters, then this is a no-op.
  if (have_waiters)
    ReleaseSemaphore (cv->sema, 1, 0);
  return 0;
}

/**
The pthread_cond_broadcast function is more complex and  requires two steps:

1. It wakes up all the threads waiting on the sema_ semaphore,
   which can be done atomically by passing the waiters_count_ to ReleaseSemaphore;

2. It then blocks on the auto-reset waiters_done_ event until
   the last thread in the group of waiting threads exits the pthread_cond_wait critical section.
**/
static int pthread_cond_broadcast (pthread_cond_t *cv)
{
  //This is needed to ensure that <waiters_count_> and <was_broadcast_> are consistent relative to each other.
  EnterCriticalSection (&cv->waiters_count_lock);
  int have_waiters = 0;

  if (cv->waiters_count > 0) {
    // We are broadcasting, even if there is just one waiter...
    // Record that we are broadcasting, which helps optimize
    // <pthread_cond_wait> for the non-broadcast case.
    cv->was_broadcast = 1;
    have_waiters = 1;
  }

  if (have_waiters)
  {
    // Wake up all the waiters atomically.
    ReleaseSemaphore (cv->sema, cv->waiters_count, 0);

    LeaveCriticalSection (&cv->waiters_count_lock);

    // Wait for all the awakened threads to acquire the counting semaphore.
    WaitForSingleObject (cv->waiters_done, INFINITE);
    // This assignment is okay, even without the <waiters_count_lock> held
    // because no other waiter threads can wake up to access it.
    cv->was_broadcast = 0;
  }
  else
    LeaveCriticalSection (&cv->waiters_count_lock);
  return 0;
}

#endif /* R_WIN */


///////////////////////////////////////////////////////////////////////////////
// ConditionVar member functions
ConditionVar::ConditionVar(Mutex* mtx) : m_ownMtx(mtx == NULL), m_mtx(mtx), m_isInitialized(false)
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
#else
  if (m_ownMtx)
  {
    m_mtx = new Mutex(false);
  }
  // TODO: else should add a check of the mutex attributes to ensure
  // that the mutex is non-recursive (required for condition variables to work correctly)

  m_isInitialized = ( pthread_cond_init(&m_cond, NULL) == 0 );
#endif
}


ConditionVar::~ConditionVar()
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
#else
  pthread_cond_destroy(&m_cond);
  if (m_ownMtx)
  {
    delete m_mtx;
  }
#endif
}

bool ConditionVar::isInitialized() const
{
  return m_isInitialized && m_mtx && m_mtx->isInitialized();
}

void ConditionVar::reltime_to_abs(unsigned int msec_rel, struct timespec *ts_abs)
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
#else
  struct timespec ts_rel;
  ts_rel.tv_sec = msec_rel / 1000;
  ts_rel.tv_nsec = (msec_rel % 1000) * 1000000;
  reltime_to_abs(&ts_rel, ts_abs);
#endif
}


void ConditionVar::reltime_to_abs(const struct timespec *ts_rel, struct timespec *ts_abs)
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
#else
  struct timeval tv;
  int retval = gettimeofday(&tv, NULL);
  if (retval == 0)
  {
    ts_abs->tv_sec = tv.tv_sec + ts_rel->tv_sec;
    ts_abs->tv_nsec = tv.tv_usec * 1000 + ts_rel->tv_nsec;
    if (ts_abs->tv_nsec >= 1000000000)
    {
      ts_abs->tv_sec += ts_abs->tv_nsec / 1000000000;
      ts_abs->tv_nsec %= 1000000000;
    }
  }
  else
  {
    // TODO: throw a suitable exception
    assert(false);
  }
#endif
}


bool ConditionVar::waitAbs(const struct timespec *ts, bool* timedout)
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
  return false;
#else

  if (m_mtx->track_owner)
  {
    if (!m_mtx->haveLock())
      return false;

    --(m_mtx->cnt);
  }

  int retval = (ts) ? pthread_cond_timedwait(&m_cond, &m_mtx->m, ts) :
  pthread_cond_wait(&m_cond, &m_mtx->m);

  if (m_mtx->track_owner)
  {
    m_mtx->thread = pthread_self();
    ++(m_mtx->cnt);
  }

  if (retval != 0 && retval != ETIMEDOUT && retval != EINTR)
  {
    // TODO: throw a suitable exception.
    assert(false);
  }

  if (timedout) *timedout = (retval == ETIMEDOUT);
  return (retval == 0) ? true : false;
#endif
}


bool ConditionVar::wait(const struct timespec *ts, bool* timedout)
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
  return false;
#else
  struct timespec ts_abs;
  reltime_to_abs(ts, &ts_abs);
  return waitAbs(&ts_abs, timedout);
#endif
}

bool ConditionVar::wait(unsigned int msec, bool* timedout)
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
  return false;
#else
  struct timespec ts_abs;
  reltime_to_abs(msec, &ts_abs);
  return waitAbs(&ts_abs, timedout);
#endif
}


void ConditionVar::signal()
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
#else
  int retval = pthread_cond_signal(&m_cond);
  if (retval != 0)
  {
    // TODO: throw a suitable exception.
    assert(false);
  }
#endif
}

void ConditionVar::broadcast()
{
#if defined(R_OS_VXWORKS)
  // #warning "TODO: implement monitor for R_OS_VXWORKS"
#else
  int retval = pthread_cond_broadcast(&m_cond);
  if (retval != 0)
  {
    // TODO: throw a suitable exception
    assert(false);
  }
#endif
}


///////////////////////////////////////////////////////////////////////////////
// Semaphore functions
Semaphore::Semaphore(int initial)
{
#ifdef R_WIN
  sem = CreateSemaphore(NULL, initial, 10000000, NULL);
#else
  // create semaphore, unshared between processes
  sem_init(&sem, 0, initial);
#endif
}

Semaphore::~Semaphore()
{
#ifdef R_WIN
  CloseHandle(sem);
#else
  sem_destroy(&sem);
#endif
}

void Semaphore::acquire()
{
#ifdef R_WIN
  WaitForSingleObject(sem, INFINITE);
#else
  sem_wait(&sem);
#endif
}

void Semaphore::release()
{
#ifdef R_WIN
  ReleaseSemaphore(sem, 1, NULL);
#else
  sem_post(&sem);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// NamedSemaphore functions
NamedSemaphore::NamedSemaphore(const char* name, unsigned int nInitial) : m_sem(NULL), m_error(0)
{
  m_sem = sem_open(name, O_CREAT, S_IRWXU, nInitial);
  if ( m_sem == SEM_FAILED )
  {
    m_error = errno;
    m_sem = NULL;
  }
}

NamedSemaphore::~NamedSemaphore()
{
  if ( m_sem ) sem_close(m_sem);
}

void NamedSemaphore::acquire()
{
  if ( m_sem )
  {
    if ( sem_wait(m_sem) == -1 )
      m_error = errno;
  }
  else
    m_error = EINVAL;
}

void NamedSemaphore::release()
{
  if ( m_sem )
  {
    if ( sem_post(m_sem) == -1 )
      m_error = errno;
  }
  else
    m_error = EINVAL;
}

bool NamedSemaphore::isInitialized() const
{
  return (m_sem != NULL)? true:false;
}

int NamedSemaphore::error() const
{
  return m_error;
}

/*static*/
void NamedSemaphore::destory(const char* name)
{
  sem_unlink(name);
}

///////////////////////////////////////////////////////////////////////////////
// EventSemaphore functions
EventSemaphore::EventSemaphore()
    #if defined(R_UNIX) && !defined(R_OS_VXWORKS)
        : mutex(false)
    #endif
{
#ifdef R_WIN
  // create event:
  // no security attrs, manual reset, initially non-signalled, no name
  sem = CreateEvent(NULL, TRUE, FALSE, NULL);
#elif defined(R_OS_VXWORKS)
  // create semaphore, unshared between processes
  sem_init(&sem, 0, 0);
#elif defined(R_UNIX)
  pthread_cond_init(&cond, NULL);
#endif
}

EventSemaphore::~EventSemaphore()
{
#ifdef R_WIN
  CloseHandle(sem);
#elif defined(R_OS_VXWORKS)
  sem_destroy(&sem);
#elif defined(R_UNIX)
  pthread_cond_destroy(&cond);
#endif
}

void EventSemaphore::wait()
{
#ifdef R_WIN
  WaitForSingleObject(sem, INFINITE);
#elif defined(R_OS_VXWORKS)
  sem_wait(&sem);
#elif defined(R_UNIX)
  MutexGuard guard(mutex);
  if (mutex.track_owner)
  {
    --(mutex.cnt);
  }
  pthread_cond_wait(&cond, &mutex.m);

  if (mutex.track_owner)
  {
    mutex.thread = pthread_self();
    ++(mutex.cnt);
  }
#endif
}

bool EventSemaphore::wait(unsigned int msec)
{
#if defined(R_WIN)
  return WaitForSingleObject(sem, msec);
#elif defined(R_OS_VXWORKS)
  // #warning "TODO: implement EventSemaphore::wait(msec) for R_OS_VXWORKS"
  return false;
#else
  struct timespec ts_rel;
  ts_rel.tv_sec = msec / 1000;
  ts_rel.tv_nsec = (msec % 1000) * 1000000;

  struct timespec ts_abs;
  ConditionVar::reltime_to_abs(&ts_rel, &ts_abs);  // timedwait uses absolute times

  MutexGuard guard(mutex);
  if (mutex.track_owner)
  {
    --(mutex.cnt);
  }
  int retval = pthread_cond_timedwait(&cond, &mutex.m, &ts_abs);
  if (mutex.track_owner)
  {
    mutex.thread = pthread_self();
    ++(mutex.cnt);
  }

  if (retval != 0 && retval != ETIMEDOUT && retval != EINTR)
  {
    // TODO: throw a suitable exception.
  }

  return (retval == 0) ? true : false;
#endif
}

void EventSemaphore::signal()
{
#ifdef R_WIN
  SetEvent(sem);
#elif defined(R_OS_VXWORKS)
  #error "Need EventSemaphore::signal for VxWorks"
#elif defined(R_UNIX)
  MutexGuard guard(mutex);
  pthread_cond_signal(&cond);
#endif
}

void EventSemaphore::pulse()
{
#ifdef R_WIN
  // since the event is manual-reset, this call releases all waiting
  // threads and resets the event to the non-signalled state
  PulseEvent(sem);
#elif defined(R_OS_VXWORKS)
  // see Butenhof pg 237
  int nVal;
  while (TRUE)
  {
    sem_getvalue(&sem, &nVal);
    if (nVal > 0)
      break;
    sem_post(&sem);
  }

#elif defined(R_UNIX)
  MutexGuard guard(mutex);
  pthread_cond_broadcast(&cond);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Thread functions
#if !defined(R_OS_VXWORKS)

int Thread::m_nActiveThreads = 0;
unsigned int Thread::s_stackSize = 0;


void Thread::setDefaultStackSize(unsigned int stackSize)
{
  s_stackSize = stackSize;
}


unsigned int Thread::getDefaultStackSize()
{
  return s_stackSize;
}


//m_detached indicates the joinable state, i.e., whether detach or join has been called
Thread::Thread() : m_running(false), m_tid(0), m_detached(false)
{
}

Thread::Thread(const Thread &t) : m_running(false), m_tid(0), m_detached(false)
{
}

Thread::~Thread()
{
  if (m_tid)
  {
    stop();
    join();
#ifdef R_WIN
    CloseHandle(m_tid);
#endif
  }
  m_tid = 0;
}


void Thread::start(unsigned int stackSize)
{
  m_bStop = false;

  m_detached = false;

  m_running=true;

  if (!stackSize)
    stackSize = s_stackSize;
  stackSize *= 1024; // convert kbytes into bytes
  if (stackSize < THREAD_STACK_MIN)
    stackSize = THREAD_STACK_MIN;
#ifdef R_WIN
  m_tid = CreateThread(NULL, stackSize, doRun, (void *)this, 0, NULL);
#else
  // schedule this thread system-wide (scheduler won't just consider
  // highest priority thread in this process)
  pthread_attr_t attrs;
  pthread_attr_init(&attrs);
  //pthread_attr_setscope(&attrs, PTHREAD_SCOPE_SYSTEM);

  pthread_attr_setstacksize(&attrs, stackSize);

  // create it
  if (pthread_create(&m_tid, &attrs, doRun, (void *)this) != 0)
  {
    cerr << "ERROR: Failed to start thread due to: " << errno << endl;
    m_running=false;
  }

  // should do something with this return value
  pthread_attr_destroy(&attrs);
#endif
}

void Thread::stop()
{
  m_bStop = true;
}

void Thread::forcestop()
{
  stop();
  if(m_tid && !m_detached)
  {
    pthread_kill(m_tid, SIGIO);
  }
}


//return from thread with return code (the exiting thread can call this itself when its job is done or m_bStop is true)
void Thread::exitThread(int exitCode)
{
#ifdef R_WIN
  ExitThread(exitCode);
#else
  pthread_exit((void *)(long)exitCode);
#endif
}


void Thread::join()
{
  if(m_tid && !m_detached)
  {
#ifdef R_WIN
    WaitForSingleObject(m_tid, INFINITE);
#else
    pthread_join(m_tid, NULL);
#endif
  }
  m_detached = true;
}


void Thread::detach()
{
  m_detached = true;
#ifndef R_WIN
  pthread_detach(m_tid);
#endif
}


bool Thread::msleep(int milliseconds)
{
  int tRemain = milliseconds;

  // Check every second whether the thread should continue running
  while( (tRemain > 0) && continueRunning() )
  {
    int t_sec = 0;
    int t_msec = 0;

    if ( tRemain >= 1000 )
    {
      t_sec = 1;
    }
    else
    {
      t_msec = tRemain;
    }

#ifdef R_WIN
    Sleep( (t_sec * 1000) + t_msec );
#else
    // usleep is _not_ MT safe in Solaris
    //::usleep(milliseconds*1000);
    struct timeval tval;
    tval.tv_sec = t_sec;
    tval.tv_usec = t_msec * 1000;
    select(0, NULL, NULL, NULL, &tval);
#endif

    tRemain -= 1000;
  }

  // if we exited early, this will be false
  return continueRunning();
}


bool Thread::setPriority(int nPriority)
{
#ifdef R_WIN
  return SetThreadPriority(m_tid, nPriority) != 0 ? true : false;
#else
  struct sched_param param;
  int policy;

  if (pthread_getschedparam(m_tid, &policy, &param) == 0)
  {
    param.sched_priority = nPriority;
    if (pthread_setschedparam(m_tid, policy, &param) == 0)
    {
      return true;
    }
  }
  // if we fall through, something bad happened
  return false;
#endif
}


#ifdef R_WIN
  DWORD WINAPI
#else
  void *
#endif
Thread::doRun(void *arg)
{
#ifdef R_UNIX
  int last_state;
  // allow the thread to be killed right away
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_state);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &last_state);
#endif

  m_nActiveThreads++;
  Thread *t=(Thread *)arg;
  t->run();
  m_nActiveThreads--;
  return NULL;
}

#endif
