/* threads_test.c - Small quick test for new C1X threads.h implementations
*                  This test might be incomplete and not up to the (changing)
*                  standard, it is however as close as possible according to
*                  the WG14 N1425 Draft 2009-11-24 (with a lot of assumptions
*                  about the behavior of the tss_*() functions
*
*
* Copyright (c) 2010, Michael Gruhn
* No rights reserved.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "threads.h"

#define TEST_TSS 0


#if _WIN32 || __WIN32__ || _WIN64
	#define sleep(n) Sleep(n*1000)
#endif

tss_t key;

void destr(void *p)
{
	p = p; /* get rid of compiler warning */
	puts("*** hey I'm the tss destructor!");
}

int print_msg_long(void *msg)
{
	int i;
	for(i=0;i<100000000;i++)
	{
		if(i%1000000==0) printf("%s",(char*)msg);
	}
	thrd_exit(100);
	return 100;
}


int print_msg_short_noyield(void *msg)
{
	int i;
	for(i=0;i<100000;i++)
	{
		if(i%1000==0) printf("%s",(char*)msg);
	}
	thrd_exit(100);
	return 100;
}


int print_msg_short(void *msg)
{
	int i;
	for(i=0;i<100000;i++)
	{
		if(i%1000==0) printf("%s",(char*)msg);
		thrd_yield();
	}
	thrd_exit(100);
	return 100;
}


int mtx_locknsleep(void *m)
{
	struct xtime xt;
	xt.sec = 4;
	xt.nsec = 500;
	assert( mtx_lock( (mtx_t*)m) == thrd_success );
	puts("*** mtx_locknsleep thread locked m and sleeps for 4 sec 500 nsec");
	thrd_sleep(&xt);
	assert( mtx_unlock( (mtx_t*)m) == thrd_success );
	puts("*** mtx_locknsleep thread unlocked m");
	thrd_exit(100);
	return 100;
}


void blah(void)
{
	puts("*** I get called only ONCE!!");
}


struct cnd_mtx_pair
{
	cnd_t c;
	mtx_t m;
};


int cnd_waitforsignal(void* cm)
{
	puts("*** cnd_waitforsignal lock cm.m");
	assert( mtx_lock(&((struct cnd_mtx_pair*)cm)->m ) == thrd_success );
	puts("*** cnd_waitforsignal cnd_wait on cm.c");
	assert(
		cnd_wait(&((struct cnd_mtx_pair*)cm)->c,&((struct cnd_mtx_pair*)cm)->m)
		== thrd_success );
	puts("*** cnd_waitforsignal signaled!");
	sleep(3);
	assert( mtx_unlock(&((struct cnd_mtx_pair*)cm)->m ) == thrd_success );
	puts("*** cnd_waitforsignal unlocks cm.m");

	return 100;
}


int cnd_timedwaitforsignal(void* cm)
{
	struct xtime xt;
	xt.sec = 1;
	xt.nsec = 100000;
	puts("*** cnd_timedwaitforsignal lock cm.m");
	assert( mtx_lock(&((struct cnd_mtx_pair*)cm)->m ) == thrd_success );
	puts("*** cnd_timedwaitforsignal cnd_wait on cm.c for 2 sec 1000 nsec");
	assert(
		cnd_timedwait(&((struct cnd_mtx_pair*)cm)->c,
			&((struct cnd_mtx_pair*)cm)->m, &xt)
			== thrd_timeout );
	puts("*** cnd_timedwaitforsignal timedout");

	assert(
		cnd_wait(&((struct cnd_mtx_pair*)cm)->c,&((struct cnd_mtx_pair*)cm)->m)
		== thrd_success );
	puts("*** cnd_timedwaitforsignal signaled!");
	assert( mtx_unlock(&((struct cnd_mtx_pair*)cm)->m ) == thrd_success );
	puts("*** cnd_timedwaitforsignal unlocks cm.m");

	return 666;
}


static mtx_t gm;

int tss_test(void *p)
{
	p = p; /* get rid of compiler warning */
	mtx_lock(&gm);
	assert( tss_create(&key, destr) == thrd_success );
	assert( tss_set(key, p) == thrd_success);
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( tss_set(key, p) == thrd_success);
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	mtx_lock(&gm);
	assert( p == tss_get(key) );
	mtx_unlock(&gm);
	puts("*** tss destructor should get called");
	thrd_exit(0);
	return 0;
}


int main()
{
	int ret;
	thrd_t t[10];

	mtx_t m;
	xtime xt;
	xt.sec = 1;
	xt.nsec = 5;

	struct cnd_mtx_pair cm;

	puts("C1X threads from pthreads test (threads.c)");
	puts("******************************************");
	puts("If this test doesn't make any progress for longer than 1 minute \
assume it failed because than it is likely a deadlock occured.");

	assert( thrd_create(&(t[0]),print_msg_long,"0") == thrd_success );
	assert(thrd_equal(thrd_current(),thrd_current())!=0);
	assert(thrd_equal(thrd_current(),t[0])==0);
	puts("thrd_current(), thrd_equal() OK");
	assert( thrd_create(&(t[1]),print_msg_long,"1") == thrd_success );
	assert( thrd_create(&(t[2]),print_msg_long,"2") == thrd_success );
	assert( thrd_create(&(t[3]),print_msg_long,"3") == thrd_success );
	assert( thrd_create(&(t[4]),print_msg_long,"4") == thrd_success );
	assert( thrd_create(&(t[5]),print_msg_long,"5") == thrd_success );

	assert( thrd_join(t[0],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[1],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[2],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[3],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[4],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[5],&ret) == thrd_success );
	assert(ret==100);
	puts(" ***");

	puts("thrd_create(), thrd_join() and thrd_exit(): OK");

	assert( thrd_create(&(t[0]),print_msg_short_noyield,"0") == thrd_success );
	assert( thrd_create(&(t[1]),print_msg_short_noyield,"1") == thrd_success );
	assert( thrd_create(&(t[2]),print_msg_short_noyield,"2") == thrd_success );
	assert( thrd_create(&(t[3]),print_msg_short_noyield,"3") == thrd_success );
	assert( thrd_create(&(t[4]),print_msg_short_noyield,"4") == thrd_success );
	assert( thrd_create(&(t[5]),print_msg_short_noyield,"5") == thrd_success );

	assert( thrd_join(t[0],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[1],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[2],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[3],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[4],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[5],&ret) == thrd_success );
	assert(ret==100);
	puts(" ***");

	assert( thrd_create(&(t[0]),print_msg_short,"0") == thrd_success );
	assert( thrd_create(&(t[1]),print_msg_short,"1") == thrd_success );
	assert( thrd_create(&(t[2]),print_msg_short,"2") == thrd_success );
	assert( thrd_create(&(t[3]),print_msg_short,"3") == thrd_success );
	assert( thrd_create(&(t[4]),print_msg_short,"4") == thrd_success );
	assert( thrd_create(&(t[5]),print_msg_short,"5") == thrd_success );

	assert( thrd_join(t[0],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[1],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[2],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[3],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[4],&ret) == thrd_success );
	assert(ret==100);
	assert( thrd_join(t[5],&ret) == thrd_success );
	assert(ret==100);
	puts(" ***");

	puts("is thrd_yield() OK? (second number set should be more fragmented than first two)");

	assert( mtx_init(&m, mtx_plain) == thrd_success );
	assert( thrd_create(&t[0],mtx_locknsleep,&m) == thrd_success );
	sleep(1);
	puts("*** main thread try lock m");
	assert( mtx_lock(&m) == thrd_success );
	puts("*** main thread locked m");
	assert( mtx_unlock(&m) == thrd_success );

	assert(thrd_join(t[0],&ret) == thrd_success );
	mtx_destroy(&m);

	puts("thrd_sleep(), mtx_init(), mtx_destroy() and mtx_plain with mtx_lock() and mtx_unlock() OK");


	assert( mtx_init(&m, mtx_timed) == thrd_success );
	assert( thrd_create(&t[0],mtx_locknsleep,&m) == thrd_success );
	sleep(1);
	puts("*** main thread try lock m for 1 sec 5 nsec");
	assert( mtx_timedlock(&m,&xt) == thrd_timeout );
	puts("*** main thread's try on m timedout");

	assert(thrd_join(t[0],&ret) == thrd_success );

	mtx_destroy(&m);

	puts("mtx_timed and mtx_timedlock() OK");


	assert( mtx_init(&m, mtx_plain|mtx_recursive) == thrd_success );
	assert( mtx_lock(&m) == thrd_success );
	puts("*** main locking recursive m: 1st time (beware of deadlock!)");
	assert( mtx_lock(&m) == thrd_success );
	puts("*** main locking recursive m: 2nd time (yay no deadlock!)");
	assert( mtx_lock(&m) == thrd_success );
	puts("*** main locking recursive m: 3rd time (is a charm)");
	puts("*** main thread unlocking m: 1");
	assert( mtx_unlock(&m) == thrd_success );
	puts("*** unlocking: 2");
	assert( mtx_unlock(&m) == thrd_success );
	puts("*** unlocking: 3");
	assert( mtx_unlock(&m) == thrd_success );

	mtx_destroy(&m);

	puts("mtx_plain | mtx_rekursive OK");


	assert( mtx_init(&m, mtx_try) == thrd_success );
	assert( mtx_lock(&m) == thrd_success );
	assert( mtx_trylock(&m) == thrd_busy );
	assert( mtx_unlock(&m) == thrd_success );

	mtx_destroy(&m);

	puts("mtx_trylock() OK");

	once_flag of = ONCE_FLAG_INIT;
	call_once(&of,blah);
	call_once(&of,blah);
	call_once(&of,blah);

	puts("was call_once() OK?");

	assert( cnd_init(&cm.c) == thrd_success );
	assert( mtx_init(&cm.m, mtx_plain) == thrd_success );
	assert( thrd_create(&t[0],cnd_waitforsignal,&cm) == thrd_success );
	sleep(1);
	puts("*** main try lock cm.m");
	assert( mtx_lock(&cm.m) == thrd_success );
	puts("*** main locked cm.m");
	puts("*** main unlock cm.m");
	assert( mtx_unlock(&cm.m) == thrd_success );
	assert( cnd_signal(&cm.c) == thrd_success );
	puts("*** main try lock cm.m");
	assert( mtx_lock(&cm.m) == thrd_success );
	puts("*** main locked cm.m");
	puts("*** main unlock cm.m");
	assert( mtx_unlock(&cm.m) == thrd_success );
	assert( thrd_join(t[0],&ret) == thrd_success );
	mtx_destroy(&cm.m);
	cnd_destroy(&cm.c);

	puts("cnd_init(), cnd_wait(), cnd_signal() and cnd_destroy() OK");

	assert( cnd_init(&cm.c) == thrd_success );
	assert( mtx_init(&cm.m, mtx_plain) == thrd_success );
	puts("*** start 2 cnd_waitforsignal threads");
	assert( thrd_create(&t[0],cnd_waitforsignal,&cm) == thrd_success );
	assert( thrd_create(&t[1],cnd_waitforsignal,&cm) == thrd_success );
	sleep(1);
	puts("*** broadcast to wake them BOTH up");
	assert( cnd_broadcast(&cm.c) == thrd_success );
	assert( thrd_join(t[0],&ret) == thrd_success );
	assert( thrd_join(t[1],&ret) == thrd_success );
	mtx_destroy(&cm.m);
	cnd_destroy(&cm.c);

	puts("cnd_broadcast() OK");


	assert( cnd_init(&cm.c) == thrd_success );
	assert( mtx_init(&cm.m, mtx_plain) == thrd_success );
	assert( thrd_create(&t[0],cnd_timedwaitforsignal,&cm) == thrd_success );
	sleep(5);
	assert( cnd_broadcast(&cm.c) == thrd_success );
	assert( thrd_join(t[0],&ret) == thrd_success );
	assert( ret == 666 );
	mtx_destroy(&cm.m);
	cnd_destroy(&cm.c);

	puts("cnd_timedwait() OK");

#if TEST_TSS
	assert( mtx_init(&gm, mtx_plain) == thrd_success );

	assert( tss_create(&key, destr) == thrd_success );

	assert( thrd_create(&t[0],tss_test,(void*)0x1111) == thrd_success );
	assert( thrd_create(&t[1],tss_test,(void*)0xffff) == thrd_success );
	assert( thrd_join(t[0],&ret) == thrd_success );
	assert( thrd_join(t[1],&ret) == thrd_success );

	tss_delete(key);
	mtx_destroy(&gm);

	puts("tss_create(), tss_set(), tss_get(), tss_delete() OK");
#else
	puts("tss_create(), tss_set(), tss_get(), tss_delete() NOT TESTED!!!");
#endif

	assert( xtime_get(&xt, 1234) == 0 );
	assert( xtime_get(&xt, TIME_UTC) == TIME_UTC);
	printf("xtime_get returns sec=%ld nsec=%ld, is this correct?\n", xt.sec, xt.nsec);

	puts("ALL OK ===============");

	return 0;
}

