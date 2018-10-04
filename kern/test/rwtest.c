/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>
#include <current.h>

#define CREATELOOPS    8
#define NTHREADS      32

static volatile unsigned long testval1;
static volatile unsigned long testval2;
static volatile unsigned long testval3;
static volatile int32_t testval4;

static struct semaphore *testsem = NULL;
static struct semaphore *donesem = NULL;
static struct rwlock *testrwlock = NULL;
static struct testwarehouse {
	unsigned long stock[64];
	int top;
}warehouse;

struct spinlock status_lock;
static bool test_status = TEST161_FAIL;

static
void
producerthread(void *junk, unsigned long num)
{
	(void)junk;

	random_yielder(4);

	kprintf_t(".");
	KASSERT(!(rwlock_do_i_hold_write(testrwlock)));

	rwlock_acquire_write(testrwlock);
	KASSERT(rwlock_do_i_hold_write(testrwlock));

	warehouse.stock[++warehouse.top] = num;
	kprintf("\n\n**Produced: %02lu | %02lu\n", curthread->id, num);

	rwlock_release_write(testrwlock);
	
	V(donesem);
}

static
void
consumerthread(void *junk, unsigned long num)
{
	(void)junk;
	(void)num;

	random_yielder(4);

	int i;

	for (i=0; i<CREATELOOPS; i++)
	{
		random_yielder(4);
		
		rwlock_acquire_read(testrwlock);
		
		while (warehouse.top == -1) {
			spinlock_acquire(&testrwlock->rlock_spinlock);
			testrwlock->r_counter--;
			wchan_sleep(testrwlock->rlock_wchan, &testrwlock->rlock_spinlock);
			testrwlock->r_counter++;
			spinlock_release(&testrwlock->rlock_spinlock);
		}

		spinlock_acquire(&testrwlock->rlock_spinlock);

		kprintf("%02lu | ", curthread->id);
 
		testrwlock->r_counter--;
		wchan_sleep(testrwlock->rlock_wchan, &testrwlock->rlock_spinlock);
		testrwlock->r_counter++;

		spinlock_release(&testrwlock->rlock_spinlock);

		rwlock_release_read(testrwlock);
	}

	V(donesem);
}

/*
 * Use these stubs to test your reader-writer locks.
 */

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

	int i, result;

	kprintf_n("Starting rwt1...");

	for (i=0; i<CREATELOOPS; i++) {
		kprintf_t(".");
		testrwlock = rwlock_create("testrwlock");
		if (testrwlock == NULL) {
			panic("rwt1: lock_create failed\n");
		}
		testsem = sem_create("testsem", 0);
		if (testsem == NULL) {
			panic("rwt1: sem_create failed\n");
		}
		donesem = sem_create("donesem", 0);
		if (donesem == NULL) {
			panic("rwt1: sem_create failed\n");
		}
		if (i != CREATELOOPS - 1) {
			rwlock_destroy(testrwlock);
			sem_destroy(testsem);
			sem_destroy(donesem);
		}
	}

	for (i=0; i<64; i++)
	{
		warehouse.stock[i] = 0;
	}

	warehouse.top = -1;

	spinlock_init(&status_lock);
	test_status = TEST161_SUCCESS;

	for (i=0; i<NTHREADS; i++) 
	{
		kprintf_t(".");
		if (i%2 == 0) 
		{
			result = thread_fork("rwt1", NULL, producerthread, NULL, i);
			if (result) 
				panic("rwt1: thread_fork failed: %s\n", strerror(result));
		}
		else 
		{
			result = thread_fork("rwt1", NULL, consumerthread, NULL, i);
			if (result)
				panic("rwt1: thread_fork failed: %s\n", strerror(result));
		}
	}


	for (i=0; i<NTHREADS; i++) 
	{
		kprintf_t(".");
		P(donesem);
	}

	rwlock_destroy(testrwlock);
	sem_destroy(donesem);
	testrwlock = NULL;
	donesem = NULL;

	kprintf_t("\n");
	
	success(test_status, SECRET, "rwt1");

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("Starting rwt2...\n");
	kprintf_n("(This test panics on success!)\n");

	testrwlock = rwlock_create("testrwlock");
	if (testrwlock == NULL) {
		panic("rwt2: rwlock_create failed\n");
	}

	secprintf(SECRET, "Should panic...", "rwt2");
	rwlock_release_write(testrwlock);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt2");

	/* Don't do anything that could panic. */

	testrwlock = NULL;
	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("Starting rwt3...\n");
	kprintf_n("(This test panics on success!)\n");

	testrwlock = rwlock_create("testrwlock");
	if (testrwlock == NULL) {
		panic("rwt3: rwlock_create failed\n");
	}

	secprintf(SECRET, "Should panic...", "rwt3");
	rwlock_release_read(testrwlock);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt2");

	/* Don't do anything that could panic. */

	testrwlock = NULL;
	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt4 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt5 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
