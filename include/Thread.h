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

#ifndef _THREAD_H
#define _THREAD_H

#include <base.h>

#if defined(R_WIN)
    #include <windows.h>
#elif defined(R_OS_VXWORKS)
    #include <vxWorks.h>
    #include <semaphore.h>
    #include <semLib.h>
    #include <taskLib.h>
#else
    #include <stdlib.h>
    #include <pthread.h>
    #include <signal.h>
    #include <semaphore.h>
    #include <sys/time.h>
    #include <assert.h>
#endif


#ifdef R_OS_LINUX
//extern "C" { int pthread_mutexattr_setkind_np(pthread_mutexattr_t*, int); }
#endif


/* On windows we have to provide our own condition variables */
#ifdef R_WIN

struct timespec
{
  long tv_sec;
  long tv_nsec;
};

typedef struct
{
  // Number of waiting threads.
  int waiters_count;

  // Serialize access to <waiters_count_>.
  CRITICAL_SECTION waiters_count_lock;

  // Semaphore used to queue up threads waiting for the condition to become signaled.
  HANDLE sema;

  // An auto-reset event used by the broadcast/signal thread to wait
  // for all the waiting thread(s) to wake up and be released from the semaphore.
  HANDLE waiters_done;

  // Keeps track of whether we were broadcasting or signaling.  This
  // allows us to optimize the code if we're just signaling.
  size_t was_broadcast;
} pthread_cond_t;

typedef HANDLE pthread_mutex_t;
#endif


#ifdef R_NAMESPACE_SUPPORTED
namespace OpenNfsC {
namespace Thread {
#endif

enum Sync_Status { Sync_Success, Sync_Alloc_Failed, Sync_Init_Failed, Sync_Invalid_Pointer };


class Mutex {
public:
  Mutex(bool recursive = true, bool errorChecking = false, bool tracking = false)
      : m_isInitialized(false), cnt(0), track_owner(tracking)
  {
#ifdef R_WIN
    m = CreateMutex(NULL, 0, NULL);
    m_isInitialized = ( m != NULL );
#elif defined(R_OS_VXWORKS)
    m = semMCreate(SEM_Q_PRIORITY);
    m_isInitialized = ( m != NULL );
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
#if defined(R_OS_SOLARIS)
    if (recursive)
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    else if (errorChecking)
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#elif defined(R_OS_LINUX)
    if (recursive)
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    else if (errorChecking)
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#endif
    m_isInitialized = ( pthread_mutex_init(&m, &attr) == 0 );
    pthread_mutexattr_destroy(&attr);
#endif
  }

  virtual ~Mutex()
  {
#ifdef R_WIN
    CloseHandle(m);
#elif defined(R_OS_VXWORKS)
    semDelete(m);
#else
    pthread_mutex_destroy(&m);
#endif
  }

  //! Check whether the object was initialized properly. Call it after the constructor.
  inline bool isInitialized() const { return m_isInitialized; }

  // Acquire this mutex. The calling thread will block until the acquire succeeds.
  // @return true on success or false on failure (will happen with error checking).
  inline bool acquire()
  {
#ifdef R_WIN
    WaitForSingleObject(m, INFINITE);
    return true;
#elif defined(R_OS_VXWORKS)
    semTake(m, WAIT_FOREVER);
    return true;
#else
    if (track_owner)
    {
      if( pthread_mutex_lock(&m) == 0 ) {
        thread=pthread_self(); /* set thread first for haveLock() */
        ++cnt;
        return true;
      }
      return false;
    }
    else
    {
      return pthread_mutex_lock(&m) == 0;
    }
#endif
  }

  // This method is a synonym for acquire(). Its use is deprecated.
  inline bool acquireWithoutLogging()
  {
    return acquire();
  }

  // Try to acquire the mutex. Never blocks.
  // @return true if the mutex has been acquired or false if the mutex could not be acquired because it's currently locked.
  inline bool tryAcquire()
  {
#ifdef R_WIN
    return WaitForSingleObject(m, 0) == WAIT_OBJECT_0;
#elif defined(R_OS_VXWORKS)
    return semTake(m, NO_WAIT) == 0;
#else
    if (track_owner)
    {
      if ( pthread_mutex_trylock(&m) == 0) {
        thread=pthread_self(); /* set thread first for haveLock() */
        ++cnt;
        return true;
      }
      return false;
    }
    else
    {
      return pthread_mutex_trylock(&m) == 0;
    }
#endif
  }

  // Release this mutex.
  // @return true on success or false on failure.
  inline bool release()
  {
#ifdef R_WIN
    ReleaseMutex(m);
    return true;
#elif defined(R_OS_VXWORKS)
    semGive(m);
    return true;
#else
    if (track_owner)
    {
      if (!haveLock())
        return false;

      --cnt;
      if (pthread_mutex_unlock(&m) != 0)
      {
        ++cnt;
        return false;
      }
      return true;
    }
    else
    {
      return pthread_mutex_unlock(&m) == 0;
    }
#endif
  }

  // This method is a synonym for release(). Its use is deprecated.
  inline bool releaseWithoutLogging()
  {
    return release();
  }

  inline bool haveLock() volatile
  {
    if (track_owner)
      return (cnt != 0) && (thread == pthread_self());
    else
      abort(); // Programmer error!
  }

  static Mutex* create(Sync_Status* pStatus = NULL)
  {
    Mutex* m = NULL; try { m = new Mutex(); } catch (...) {}
    Sync_Status status = (!m)? Sync_Alloc_Failed: (!m->isInitialized())? Sync_Init_Failed:Sync_Success;
    if (status != Sync_Success && m) { delete m; m = NULL; }
    if ( pStatus ) *pStatus = status;
    return m;
  }

protected:
  friend class ConditionVar;
  friend class EventSemaphore;

#if defined(R_WIN)
  HANDLE m;
#elif defined(R_OS_VXWORKS)
  SEM_ID m;
#else
  pthread_mutex_t m;
  bool m_isInitialized;
  volatile int cnt;
  volatile pthread_t thread;
  const bool track_owner;
#endif
};


/**
 * Condition variable class (a.k.a. monitor). Basically a mutex
 * with wait/signal/broadcast functionality.
 **/
class ConditionVar
{
  public:
    ConditionVar(Mutex* mtx = NULL);
    virtual ~ConditionVar();

    //! Check whether the object was initialized properly. Call it after the constructor.
    bool isInitialized() const;

    // Populate a timespec with the current time plus the specified
    // relative time (in milliseconds).
    // @param msec_rel [in] relative time in the future (milliseconds)
    // @param ts_abs [out] timespec that is populated with the absolute time. Must not be NULL.
    static void reltime_to_abs(unsigned int msec_rel, struct timespec *ts_abs);

    // Populate a timespec with the current time plus the specified relative timespec.
    // @param ts_rel [in] relative time in the future (timespec)
    // @param ts_abs [out] timespec that is populated with the absolute time. Must not be NULL.
    static void reltime_to_abs(const struct timespec *ts_rel, struct timespec *ts_abs);

    // Wait on this condition variable until it has been
    // signalled or until the absolute timeout has expired.
    // @param ts_abs [in] absolute time the timeout will occur.
    //     NULL means wait forever.
    // @param timedout [out] If non-NULL this will be true if the timeout has
    //     expired or false if the system call was interrupted. Defined only
    //     if this method returns false.
    // @return true on success or false if the timeout has expired
    //     or the system call was interrupted (check timedout).
    bool waitAbs(const struct timespec *ts_abs, bool* timedout = NULL);

    // Wait on this condition variable until it has been signalled
    // or until the relative timeout has expired.
    // @param ts_rel [in] relative time the timeout will occur.
    //     Must not be NULL.
    // @param timedout [out] If non-NULL this will be true if the timeout has
    //     expired or false if the system call was interrupted. Defined
    //     only if this method returns false.
    // @return true on success or false if the timeout has expired
    //     or the system call was interrupted (check timedout).
    bool wait(const struct timespec *ts_rel, bool* timedout = NULL);

    // Wait on this condition variable until it has been signalled
    // or until the relative timeout has expired.
    // @param msec_rel [in] relative time the timeout will occur in
    //     milliseconds. Must not be NULL.
    // @param timedout [out] If non-NULL this will be true if the timeout has
    //     expired or false if the system call was interrupted. Defined
    //     only if this method returns false.
    // @return true on success or false if the timeout has expired
    //     or the system call was interrupted (check timedout).
    bool wait(unsigned int msec_rel, bool* timedout = NULL);

    // Wait forever for this condition variable to be signalled.
    // @return true on success or false if the system call was interrupted.
    bool wait(){ return waitAbs(NULL);}

    void signal();
    void broadcast();

  private:
    ConditionVar(const ConditionVar&);
    ConditionVar& operator=(const ConditionVar&);

  protected:
    friend class MutexGuard;

#if defined(R_OS_VXWORKS)
// #warning "TODO: implement ConditionVar for R_OS_VXWORKS"
#else
    bool m_ownMtx;
    Mutex *m_mtx;
    pthread_cond_t m_cond;
    bool m_isInitialized;
#endif
};


/**
 * @brief Event declaration
 */
class Event : public ConditionVar
{
public:
  //! Mutex object associated with the condition variable
  mutable Mutex mutex;

  //! Default constructor.
  Event() : ConditionVar(&mutex), mutex(false) {}
  //! Destructor.
  virtual ~Event() {}

  //! Check whether the mutex and the condition variable objects were initialized properly.
  //! Call it after the constructor.
  bool isInitialized() const
    { return ( mutex.isInitialized() && ConditionVar::isInitialized() ); }

  //! Create a new instance of the Event object. It makes sure that the object is initialized.
  static Event* create(Sync_Status* pStatus = NULL)
  {
    Event* e = NULL; try { e = new Event(); } catch (...) {}
    Sync_Status status = (!e)? Sync_Alloc_Failed:
                                 (!e->isInitialized())? Sync_Init_Failed:Sync_Success;
    if (status != Sync_Success && e) { delete e; e = NULL; }
    if ( pStatus ) *pStatus = status;
    return e;
  }
};

class MutexGuard {
public:
    MutexGuard(Mutex* mutex) : m_pMutex(mutex)  { m_pMutex->acquire(); }
    MutexGuard(Mutex& mutex) : m_pMutex(&mutex) { m_pMutex->acquire(); }

#if defined(R_WIN)
// #warning "TODO: implement ConditionVar for R_WIN"
#elif defined(R_OS_VXWORKS)
// #warning "TODO: implement ConditionVar for R_OS_VXWORKS"
#else
    MutexGuard(ConditionVar* cv) : m_pMutex(cv->m_mtx) { m_pMutex->acquire(); }
    MutexGuard(ConditionVar& cv) : m_pMutex(cv.m_mtx)  { m_pMutex->acquire(); }
#endif

    ~MutexGuard() { m_pMutex->release(); }
private:
    Mutex*  m_pMutex;
};


template <class T> class AutoLock
{
public:
  AutoLock(T& syncObj) : m_syncObj(&syncObj) { if ( m_syncObj ) m_syncObj->acquire(); }
  AutoLock(T* syncObj) : m_syncObj(syncObj) { if ( m_syncObj ) m_syncObj->acquire(); }
  ~AutoLock() { if ( m_syncObj ) m_syncObj->release(); }

private:
  T* m_syncObj;
};



class Semaphore
{
  public:
    Semaphore(int nInitial);
    ~Semaphore();
    void acquire();
    void release();

  private:
    #if defined(R_WIN)
        HANDLE sem;
    #elif defined(R_OS_VXWORKS)
        sem_t sem;
    #else
        sem_t sem;
    #endif
};


class NamedSemaphore
{
  public:
    NamedSemaphore(const char* name, unsigned int nInitial);
    ~NamedSemaphore();
    void acquire();
    void release();

    bool isInitialized() const;
    int error() const;

    static void destory(const char* name);

  private:
    sem_t*      m_sem;   //! semaphore object
    int         m_error; //! last error
};


class EventSemaphore
{
  public:
    EventSemaphore();
    ~EventSemaphore();

    void wait();
    bool wait(unsigned int msec_rel);

    void signal();
    void pulse();

  private:
    #if defined(R_WIN)
        HANDLE sem;
    #elif defined(R_OS_VXWORKS)
        sem_t sem;
    #elif defined(R_UNIX)
        Mutex mutex;
        pthread_cond_t cond;
    #endif
};


#if !defined(R_OS_VXWORKS)
class Thread
{
  public:
    Thread();
    Thread(const Thread &t);
    virtual ~Thread();

    // Start this thread running.
    // NOTE: This function should not be called more than once
    //  per object instance unless the user calls join or detach between starts.
    // @param stackSize thread stack size hint in kbytes (use 0 for the operating system default).
    void start(unsigned int stackSize = 0);

    // Set the default stack size for new threads.
    // @param stackSize default stack size in kbytes (use 0 for the operating system default).
    static void setDefaultStackSize(unsigned int stackSize);

    // @return the default stack size for new threads (0 means the operating system default will be used).
    static unsigned int getDefaultStackSize();

    void stop();
    void forcestop();
    void exitThread(int exitCode); // for thread to terminate itself
    void join();
    void detach();

    bool isRunning() const { return m_running; }

    #ifdef R_WIN
      typedef HANDLE ThreadHandle_t ;
    #else
      typedef pthread_t ThreadHandle_t;
    #endif

    // returns true if successful, handle needs to be closed by closeClonedThreadHandle();
    static bool cloneCurrentThreadHandle(ThreadHandle_t & handle)
    {
      #ifdef R_WIN
        return 0 != DuplicateHandle(
                    GetCurrentProcess(),  // source process
                    GetCurrentThread(),   // source handle
                    GetCurrentProcess(),  // target process
                    &handle ,             // target handle
                    0,                    // desired access, ignored because of DUPLICATE_SAME_ACCESs
                    FALSE,                // not inheritable
                    DUPLICATE_SAME_ACCESS // option
                    );
      #else
        handle = pthread_self();
        return true;
      #endif
    }

    static void closeClonedThreadHandle(ThreadHandle_t handle)
    {
      #ifdef R_WIN
        CloseHandle(handle);
      #else
        // no action needed for pthread
      #endif
    }

  protected:
    virtual void run() = 0;
    bool setPriority(int nPriority);
    bool continueRunning() const { return !m_bStop; }
    virtual bool msleep(int milliseconds);
    ThreadHandle_t getThreadHandle() const { return m_tid ; }
    // since doRun doesn't clear m_running, this can be called by the derived class before exiting
    void runExiting() { m_running = false; }

  private:
    static int m_nActiveThreads;
    static unsigned int s_stackSize;
    bool m_bStop;
    bool m_running;

    #ifdef R_WIN
      HANDLE      m_tid;
      static DWORD WINAPI doRun(void *arg);
    #else
      pthread_t   m_tid;
      static void *doRun(void *arg);
    #endif

    // m_detached indicates the joinable state, i.e., whether detach or join has been called
    bool m_detached;
  };
#else
  #warning "Thread class not implemented."
#endif

#ifdef R_NAMESPACE_SUPPORTED
}
}
#endif

#endif /* _THREAD_H */
