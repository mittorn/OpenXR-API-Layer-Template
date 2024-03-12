#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)
#define noinline __attribute__ ((noinline))
#define forceinline __attribute__ ((always_inline))
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#define noinline
#define forceinline
#define __PRETTY_FUNCTION__ "$"
#endif // __GNUC__
#ifdef _WIN32
// todo: test, optimize
#include <windows.h>
struct Mutex
{
	CRITICAL_SECTION mutex;

	inline void Lock()
	{
		EnterCriticalSection(&mutex);
	}
	inline void TryLock()
	{
		TryEnterCriticalSection(&mutex);
	}
	inline void Unlock()
	{
		LeaveCriticalSection(&mutex);
	}
	Mutex(){
		InitializeCriticalSection(&mutex);
	}
};

using SpinLock = Mutex;

struct Thread
{
	HANDLE thread_id = 0;
	void (*func)(void *u);
	Thread(void (*f)(void*), void *u = nullptr): func(f), userdata(u){}
	void *userdata;

	static DWORD WINAPI Runner(LPVOID p)
	{
		Thread *t = (Thread*)p;
		t->func(t->userdata);
		ExitThread(0);
		return 0;
	}
	void Start()
	{
		if(!thread_id)
			thread_id = CreateThread( NULL, 0, &Thread::Runner, this, 0, NULL);
	}
	void Join()
	{
		if(thread_id)
			WaitForSingleObject(thread_id, INFINITE);
		thread_id = 0;
	}
	void RequestStop()
	{
		if(thread_id)
			CancelSynchronousIo(thread_id);
	}
	~Thread(){
		RequestStop();
		Join();
	}
};

forceinline static void SyncBarrier()
{
	MemoryBarrier();
}

template <typename T>
forceinline static inline T Fetch(volatile T& t)
{
	MemoryBarrier();
	return t;
}

#else

#include <signal.h>
#include <pthread.h>

struct SpinLock
{
	volatile int _locked = 0;
	forceinline inline void Lock()
	{
		while(__sync_lock_test_and_set(&_locked, 1))
			sched_yield();
	}
	forceinline inline bool TryLock()
	{
		return __sync_lock_test_and_set(&_locked, 1);
	}
	forceinline inline void Unlock()
	{
		__sync_lock_release(&_locked);
	}
};

struct Mutex
{
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	forceinline inline void Lock()
	{
		pthread_mutex_lock(&mutex);
	}
	forceinline inline void TryLock()
	{
		pthread_mutex_trylock(&mutex);
	}
	forceinline inline void Unlock()
	{
		pthread_mutex_unlock(&mutex);
	}
};


struct Thread
{
	pthread_t thread_id = 0;
	void (*func)(void *u);
	Thread(void (*f)(void*), void *u = nullptr): func(f), userdata(u){}
	void *userdata;

	static void *Runner(void *p)
	{
		Thread *t = (Thread*)p;

		// Thread-specific signal handling
		struct sigaction sa;
		sa.sa_handler = [](int) {printf("USR1\n"); };
		sa.sa_flags = 0;
		sigaction(SIGUSR1, &sa, nullptr);
		t->func(t->userdata);
		return nullptr;
	}
	void Start()
	{
		if(!thread_id)
			pthread_create(&thread_id, NULL, Runner, this);
	}
	void Join()
	{
		if(thread_id)
			pthread_join(thread_id,NULL);
		thread_id = 0;
	}
	void RequestStop()
	{
		if(thread_id)
			pthread_kill(thread_id, SIGUSR1);
	}
	~Thread(){
		RequestStop();
		Join();
	}
};

forceinline static inline void SyncBarrier()
{
	 __sync_synchronize();
}

template <typename T>
forceinline static inline T Fetch(volatile T& t)
{
	return __sync_fetch_and_add(&t, (T)0);
}

#endif

// concepts might be useful...
template <typename T>
struct Lock
{
	T &lock;
	Lock(T& l): lock(l)
	{
		lock.Lock();
	}
	~Lock()
	{
		lock.Unlock();
	}
};

template <typename T, typename F>
void TryLock(T &lock, F func)
{
	if(lock.TryLock())
	{
		func();
		lock.Unlock();
	}
}

#endif // THREAD_UTILS_H
