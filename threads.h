/* threads.h - C1X threads as per WG14 N1425 Draft 2009-11-24 ISO/IEC 9899:201x
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

#ifndef THREADS_H
#define THREADS_H

#include <windows.h>


typedef struct xtime
{
	time_t sec;
	long nsec;
} xtime;

enum
{
	thrd_success = 0,
	thrd_nomem = -1,
	thrd_timeout = -2,
	thrd_busy = -3,
	thrd_error = -4
};

typedef int (*thrd_start_t)(void*);
typedef struct thrd_t
{
	HANDLE h;
	DWORD id; /* GetThreadID only supported in > VISTA
		but we need XP compatibility so we must save the IDs */
} thrd_t;

enum
{
	mtx_plain = 0x1,
	mtx_try = 0x1<<1,
	mtx_timed = 0x1<<2,
	mtx_recursive = 0x1<<3
};

typedef struct cnd_t
{
	HANDLE s;
	HANDLE m_wait;
	HANDLE m_broadcast;
	volatile LONG waiters;
} cnd_t;
typedef struct mtx_t
{
	int v;
	HANDLE m;
} mtx_t;

#define ONCE_FLAG_INIT {0}

#pragma pack(push,4) /* align for use in InterlockedCompareExchange() */
struct once_flag
{
	LONG v;
};
#pragma pack(pop)

typedef struct once_flag once_flag;


void call_once(once_flag *flag, void(*func)(void));

int cnd_broadcast(cnd_t *cond);
void cnd_destroy(cnd_t *cond);
int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const xtime *xt);
int cnd_wait(cnd_t *cond, mtx_t *mtx);


int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
thrd_t thrd_current(void);
int thrd_detach(thrd_t thr);
int thrd_equal(thrd_t thr0, thrd_t thr1);
void thrd_exit(int res);
int thrd_join(thrd_t thr, int *res);
void thrd_sleep(const xtime *xt);
void thrd_yield(void);


void mtx_destroy(mtx_t *mtx);
int mtx_init(mtx_t *mtx, int type);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *mtx, const xtime *xt);
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);


#define TSS_DTOR_ITERATIONS 0

typedef int tss_t;

typedef void(*tss_dtor_t)(void*);

int tss_create(tss_t *key, tss_dtor_t dtor);
void tss_delete(tss_t key);
void *tss_get(tss_t key);
int tss_set(tss_t key, void *val);

/* XXX: not sure what the TIME_UTC thing is good for, probably a boost thing
* XXX: Dinkumware has it enum'd however the draft doesn't make any mention
* whether this should be an enum or a PP macro .. anyway P.J. knows best so
* we go with the enum
*/
enum
{
	TIME_UTC = 1
};
int xtime_get(xtime *xt, int base);

#endif /* THREADS_H */


