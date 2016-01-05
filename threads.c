/* threads.c - C1X threads draft implementation as per
*             WG14 N1425 Draft 2009-11-24 ISO/IEC 9899:201x
*             http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1425.pdf
*             Using Win32 API as backend
*
* Copyright (c) 2010, Michael Gruhn <michael-gruhn@web.de>
* Some rights reserved.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
/* XXX: figure out if threads can be detached or disposed in windows
* (thrd_detach())
*/
/* XXX: xtime_get currently just returns a parsed GetSystemTime(), highly doubt
* this is correct
*/
/* TODO: Fix warnings, clean up code */

#include <time.h>

#include "threads.h"


void call_once(once_flag *flag, void(*func)(void))
{
	if ( InterlockedCompareExchange(&flag->v,1,0) == 0)
	{
		func();
	}
}


int cnd_broadcast(cnd_t *cond)
{
	if ( WaitForSingleObject(cond->m_broadcast, INFINITE) != 0 )
	{
		return thrd_error;
	}
	if ( WaitForSingleObject(cond->m_wait, INFINITE) != 0 )
	{
		return thrd_error;
	}
	if( cond->waiters == 0 )
	{
		return thrd_success;
	}
	if( ReleaseSemaphore(cond->s,cond->waiters,NULL) == 0 )
	{
		return thrd_error;
	}
	(cond->waiters) = 0;
	if ( ReleaseMutex(cond->m_wait) == 0 )
	{
		return thrd_error;
	}
	while(cond->waiters > 0);
	if ( ReleaseMutex(cond->m_broadcast) == 0 )
	{
		return thrd_error;
	}
	return thrd_success;
}


void cnd_destroy(cnd_t *cond)
{
	CloseHandle(cond->s);
	CloseHandle(cond->m_wait);
	CloseHandle(cond->m_broadcast);
}


#define _THREADS_H_CND_MAX_COUNT 10000
/* XXX: set this to maximum count allowed in semaphore
should be around max of LONG */

int cnd_init(cnd_t *cond)
{
	cond->waiters = 0;
	if ((cond->s = CreateSemaphore(NULL,0,_THREADS_H_CND_MAX_COUNT,NULL)) == NULL )
	{
		return thrd_error;
	}
	if ( (cond->m_wait = CreateMutex(NULL,FALSE,NULL)) == NULL )
	{
		return thrd_error;
	}
	if ( (cond->m_broadcast = CreateMutex(NULL,FALSE,NULL)) == NULL )
	{
		return thrd_error;
	}
	return thrd_success;
}


int cnd_signal(cnd_t *cond)
{
	if ( WaitForSingleObject(cond->m_wait, INFINITE) != 0 )
	{
		return thrd_error;
	}
	if( cond->waiters == 0 )
	{
		return thrd_success;
	}
	if( ReleaseSemaphore(cond->s,1,NULL) == 0 )
	{
		return thrd_error;
	}
	--(cond->waiters);
	if ( ReleaseMutex(cond->m_wait) == 0 )
	{
		return thrd_error;
	}
	return thrd_success;
}

#include <assert.h>

int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const xtime *xt)
{
	if ( WaitForSingleObject(cond->m_broadcast, INFINITE) != 0 )
	{
		return thrd_error;
	}
	if ( WaitForSingleObject(cond->m_wait, INFINITE) != 0 )
	{
		return thrd_error;
	}
	assert( cond->waiters < _THREADS_H_CND_MAX_COUNT );
	++(cond->waiters);
	if ( ReleaseMutex(cond->m_wait) == 0 )
	{
		return thrd_error;
	}
	if ( ReleaseMutex(cond->m_broadcast) == 0 )
	{
		return thrd_error;
	}
	if( mtx_unlock(mtx) != thrd_success )
	{
		return thrd_error;
	}
	DWORD r;
	r = WaitForSingleObject(cond->s, xt->sec*1000+xt->nsec/1000);
	if( r != 0 )
	{
		if( r!=WAIT_TIMEOUT)
		{
			return thrd_error;
		}
		else
		{
			if ( WaitForSingleObject(cond->m_wait, INFINITE) != 0 )
			{
				return thrd_error;
			}
			r = WaitForSingleObject(cond->s,0);
			if( r == 0 )
			{
				if ( ReleaseMutex(cond->m_wait) == 0 )
				{
					return thrd_error;
				}
				if( mtx_lock(mtx) != thrd_success)
				{
					return thrd_error;
				}
				return thrd_success;
			}
			--(cond->waiters);
			if ( ReleaseMutex(cond->m_wait) == 0 )
			{
				return thrd_error;
			}
			if( mtx_lock(mtx) != thrd_success)
			{
				return thrd_error;
			}
			return thrd_timeout;
		}
	}
	if( mtx_lock(mtx) != thrd_success)
	{
		return thrd_error;
	}
	return thrd_success;
}

#include <assert.h>

int cnd_wait(cnd_t *cond, mtx_t *mtx)
{
	if ( WaitForSingleObject(cond->m_broadcast, INFINITE) != 0 )
	{
		return thrd_error;
	}
	if ( WaitForSingleObject(cond->m_wait, INFINITE) != 0 )
	{
		return thrd_error;
	}
	assert( cond->waiters < _THREADS_H_CND_MAX_COUNT );
	++(cond->waiters);
	if ( ReleaseMutex(cond->m_wait) == 0 )
	{
		return thrd_error;
	}
	if ( ReleaseMutex(cond->m_broadcast) == 0 )
	{
		return thrd_error;
	}
	if( mtx_unlock(mtx) != thrd_success )
	{
		return thrd_error;
	}
	if ( WaitForSingleObject(cond->s, INFINITE) != 0 )
	{
		return thrd_error;
	}
	if( mtx_lock(mtx) != thrd_success)
	{
		return thrd_error;
	}
	return thrd_success;
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	thr->h = CreateThread(NULL, 0, func, arg, 0, &thr->id);
	return thr->h!=NULL?thrd_success:thrd_error;
}


thrd_t thrd_current(void)
{
	thrd_t t;
	t.h = GetCurrentThread();
	t.id = GetCurrentThreadId();
	return t;
}


int thrd_detach(thrd_t thr)
{
	/* XXX: figure out if threads can be detached or disposed in windows */
	return thrd_success;
}


int thrd_equal(thrd_t thr0, thrd_t thr1)
{
	return thr0.id == thr1.id;
}


void thrd_exit(int res)
{
	ExitThread(res);
}


int thrd_join(thrd_t thr, int *res)
{
	if ( WaitForSingleObject(thr.h,INFINITE) != 0)
	{
		return thrd_error;
	}
	if (GetExitCodeThread(thr.h,res) == 0)
	{
		return thrd_error;
	}
	CloseHandle(thr.h);
	return thrd_success;
}


void thrd_sleep(const xtime *xt)
{
	Sleep(xt->sec*1000+xt->nsec/1000);
}


void thrd_yield(void)
{
	SwitchToThread();
}




void mtx_destroy(mtx_t *mtx)
{
	CloseHandle(mtx->m);
}


int mtx_init(mtx_t *mtx, int type)
{
	mtx->v = type&mtx_recursive?-1:0;

	mtx->m = CreateMutex(NULL, FALSE, NULL);
	return mtx->m==NULL?thrd_error:thrd_success;
}

#include <assert.h>

int mtx_lock(mtx_t *mtx)
{
	DWORD r;
	r = WaitForSingleObject(mtx->m, INFINITE);
	if ( mtx->v > 0 )
	{
		assert("Deadlock"==NULL);
		for (;;); /* thread already has mutex so it can't release it so we dead lock */
	}
	if ( mtx->v == 0 )
	{
		mtx->v = 1;
	}
	return r==0?thrd_success:thrd_error;
}


int mtx_timedlock(mtx_t *mtx, const xtime *xt)
{
	DWORD r;
	r = WaitForSingleObject(mtx->m,xt->sec*1000+xt->nsec/1000); /* XXX: not 100% accurate but close enough */
	if ( mtx->v > 0 )
	{
		/* thread already has mutex so it can't release it in time so we wait for timeout */
		Sleep(xt->sec*1000+xt->nsec/1000);
		return thrd_timeout;
	}
	if ( mtx->v == 0 )
	{
		mtx->v==1;
	}
	return r==0?thrd_success:r==WAIT_TIMEOUT?thrd_timeout:thrd_error;
}


int mtx_trylock(mtx_t *mtx)
{
	DWORD r;
	r = WaitForSingleObject(mtx->m,0);
	switch (r)
	{
	case 0:
		if ( mtx->v > 0 )
		{
			return thrd_busy;
		}
		if ( mtx->v == 0)
		{
			mtx->v == 1;
			return thrd_success;
		}
	case WAIT_TIMEOUT:
	case WAIT_ABANDONED:
		return thrd_busy;
	default:
		return thrd_error;
	}
}


int mtx_unlock(mtx_t *mtx)
{
	if ( mtx->v > 0 )
	{
		mtx->v = 0;
	}
	return ReleaseMutex(mtx->m)==0?thrd_error:thrd_success;
}




int tss_create(tss_t *key, tss_dtor_t dtor)
{
	return thrd_error;
}


void tss_delete(tss_t key)
{
}


void *tss_get(tss_t key)
{
	return NULL;
}


int tss_set(tss_t key, void *val)
{
	return thrd_error;
}


int xtime_get(xtime *xt, int base)
{
	if( base != TIME_UTC )
	{
		return 0;
	}
	SYSTEMTIME t;
	GetSystemTime(&t);
	/* XXX: return some random time need to figure out what it acutally should return */
	xt->nsec = t.wMilliseconds*1000;
	xt->sec = t.wSecond+60*(t.wMinute+60*(t.wHour+24*t.wDay));
	return base;
}


