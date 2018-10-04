/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lock_name = kstrdup(name);

	if (lock->lock_name == NULL) {
		kfree(lock);
		return NULL;
	}

	lock->lock_wchan = wchan_create(lock->lock_name);
	if (lock->lock_wchan == NULL) {
		kfree(lock->lock_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->lock_spinlock);

	lock->lock_held = 0;
	lock->id = -1;

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lock_name);

	// add stuff here as needed

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);
	
	if (lock->lock_held == 1)
	{
		panic("Lock is Acquired. Cannot Destroy...\n");
	}

	spinlock_cleanup(&lock->lock_spinlock);
	wchan_destroy(lock->lock_wchan);
	kfree(lock->lock_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	KASSERT (lock != NULL);

	spinlock_acquire(&lock->lock_spinlock);
	HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);

	while (lock->lock_held == 1)
	{
		wchan_sleep(lock->lock_wchan, &lock->lock_spinlock);
	}

	if (lock->lock_held == 0)
	{
		lock->lock_held = 1;
		lock->id = curthread->id;
		HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);

		spinlock_release(&lock->lock_spinlock);
	}
}

void
lock_release(struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));
	
	spinlock_acquire(&lock->lock_spinlock);
	
	lock->lock_held = 0;
	lock->id = -1;

	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);

	wchan_wakeone(lock->lock_wchan, &lock->lock_spinlock);

	spinlock_release(&lock->lock_spinlock);
}

bool
lock_do_i_hold(struct lock *lock)
{
	if (lock->lock_held == 1 && lock->id == curthread->id)
		return true;
	else return false;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_spinlock);

	cv->cv_threads_waiting = 0;

	// add stuff here as needed

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	if (cv->cv_threads_waiting > 0)
	{
		panic("Thread(s) Active. Cannot Destroy CV...\n");
	}

	// add stuff here as needed

	spinlock_cleanup(&cv->cv_spinlock);
	wchan_destroy(cv->cv_wchan);
	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(cv != NULL);

	KASSERT(lock_do_i_hold(lock));
	
	lock_release(lock);

	spinlock_acquire(&cv->cv_spinlock);
	cv->cv_threads_waiting++;
	wchan_sleep(cv->cv_wchan, &cv->cv_spinlock);
	cv->cv_threads_waiting--;
	spinlock_release(&cv->cv_spinlock);

	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeone(cv->cv_wchan, &cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeall(cv->cv_wchan, &cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
}

////////////////////////////////////////////////////////////
//
// RWLOCKS

struct rwlock * rwlock_create(const char *name)
{
	struct rwlock *rwlock;

	int i;

	rwlock = kmalloc(sizeof(*rwlock));
	if (rwlock == NULL) {
		return NULL;
	}

	rwlock->rwlock_name = kstrdup(name);

	if (rwlock->rwlock_name == NULL) {
		kfree(rwlock);
		return NULL;
	}

	rwlock->rlock_wchan = wchan_create(rwlock->rwlock_name);
	if (rwlock->rlock_wchan == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->wlock_wchan = wchan_create(rwlock->rwlock_name);
	if (rwlock->wlock_wchan == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	spinlock_init(&rwlock->rlock_spinlock);
	spinlock_init(&rwlock->wlock_spinlock);

	rwlock->wlock_held = 0;
	rwlock->w_id = -1;
	rwlock->r_counter = 0;
	rwlock->rlock_held = -1;
	for (i=0; i<RTHREADS; i++) {
		rwlock->r_id[i] = -1;
	}

	HANGMAN_LOCKABLEINIT(&rwlock->rwlk_hangman, rwlock->rwlock_name);

	// add stuff here as needed

	return rwlock;
}

void rwlock_destroy(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	
	if (rwlock->r_counter > 0 || rwlock->wlock_held == 1)
	{
		panic("Lock is Acquired. Cannot Destroy...\n");
	}

	spinlock_cleanup(&rwlock->rlock_spinlock);
	spinlock_cleanup(&rwlock->wlock_spinlock);

	wchan_destroy(rwlock->rlock_wchan);
	wchan_destroy(rwlock->wlock_wchan);

	kfree(rwlock->rwlock_name);
	kfree(rwlock);
}

void 
rwlock_acquire_read(struct rwlock *rwlock)
{
	KASSERT (rwlock != NULL);
	KASSERT (rwlock->r_counter >= 0);

	spinlock_acquire(&rwlock->rlock_spinlock);

	while (rwlock->wlock_held == 1) {
		wchan_sleep(rwlock->rlock_wchan, &rwlock->rlock_spinlock);
	}
	
	rwlock->r_id[++rwlock->rlock_held] = curthread->id;
	rwlock->r_counter++;

	spinlock_release(&rwlock->rlock_spinlock);
}

void 
rwlock_release_read(struct rwlock *rwlock)
{
	KASSERT (rwlock != NULL);
	KASSERT (rwlock->r_counter > 0);

	int result = 0; 
	int i;

	spinlock_acquire(&rwlock->rlock_spinlock);

	for (i=0; i<RTHREADS; i++) {
		if (rwlock->r_id[i] == curthread->id) {
			result = i;
		}
	}

//	if (result > 0) 
	{
		rwlock->r_id[result] = -1;
		rwlock->r_counter--;
	}
//	else panic("Cannot release Read Lock. Never acquired it. | %02lu", curthread->id);

	spinlock_release(&rwlock->rlock_spinlock);

	spinlock_acquire(&rwlock->wlock_spinlock);
	wchan_wakeone(rwlock->wlock_wchan, &rwlock->wlock_spinlock);
	spinlock_release(&rwlock->wlock_spinlock);
}

void 
rwlock_acquire_write(struct rwlock *rwlock)
{
	KASSERT (rwlock != NULL);

	spinlock_acquire(&rwlock->wlock_spinlock);
	HANGMAN_WAIT(&curthread->t_hangman, &rwlock->rwlk_hangman);

	while (rwlock->wlock_held == 1 || rwlock->r_counter > 0)
	{
		wchan_sleep(rwlock->wlock_wchan, &rwlock->wlock_spinlock);
	}

	if (rwlock->wlock_held == 0 && rwlock->r_counter == 0)
	{
		rwlock->wlock_held = 1;
		rwlock->w_id = curthread->id;
		HANGMAN_ACQUIRE(&curthread->t_hangman, &rwlock->rwlk_hangman);

		spinlock_release(&rwlock->wlock_spinlock);
	}
}

void 
rwlock_release_write(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	KASSERT(rwlock_do_i_hold_write(rwlock));
	
	spinlock_acquire(&rwlock->wlock_spinlock);
	
	rwlock->wlock_held = 0;
	rwlock->w_id = -1;

	HANGMAN_RELEASE(&curthread->t_hangman, &rwlock->rwlk_hangman);
	
	spinlock_acquire(&rwlock->rlock_spinlock);
	wchan_wakeall(rwlock->rlock_wchan, &rwlock->rlock_spinlock);
	spinlock_release(&rwlock->rlock_spinlock);

	wchan_wakeone(rwlock->wlock_wchan, &rwlock->wlock_spinlock);

	spinlock_release(&rwlock->wlock_spinlock);
}

bool
rwlock_do_i_hold_write(struct rwlock *rwlock)
{
	if (rwlock->wlock_held == 1 && rwlock->w_id == curthread->id)
		return true;
	else return false;
}
