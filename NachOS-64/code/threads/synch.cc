// synch.cc
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(const char *debugName, int initialValue)
{
    name = (char *)debugName;
    value = initialValue;
    queue = new List<Thread *>;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // disable interrupts

    while (value == 0)
    {                                 // semaphore not available
        queue->Append(currentThread); // so go to sleep
        currentThread->Sleep();
    }
    value--; // semaphore available,
             // consume its value

    interrupt->SetLevel(oldLevel); // re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = queue->Remove();
    if (thread != NULL) // make thread ready, consuming the V immediately
        scheduler->ReadyToRun(thread);
    value++;
    interrupt->SetLevel(oldLevel);
}

#ifdef USER_PROGRAM
//----------------------------------------------------------------------
// Semaphore::Destroy
// 	Destroy the semaphore, freeing the waiting threads
//	This is used to destroy a user semaphore
//----------------------------------------------------------------------

void Semaphore::Destroy()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    while ((thread = queue->Remove()) != NULL) // make thread ready
        scheduler->ReadyToRun(thread);

    interrupt->SetLevel(oldLevel);
}

#endif

//----------------------------------------------------------------------
// Lock::Lock
// 	Initialize a Lock, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//----------------------------------------------------------------------
Lock::Lock(const char *debugName)
{
    name = (char *)debugName;       //for debugging
    lockHolder = NULL;              //Lock is FREE
    mutex = new Semaphore(name, 1); //Semaphore is available
}

//----------------------------------------------------------------------
// Lock::Lock
// 	Free lock.
//  De-allocate semaphore, when no longer needed.
//----------------------------------------------------------------------
Lock::~Lock()
{
    delete mutex; //Delete Semaphore
}

//----------------------------------------------------------------------
// Lock::Acquire
// 	Allows any process to acquire the lock
//	in order to access shared variables.
//----------------------------------------------------------------------
void Lock::Acquire()
{
    mutex->P(); //Call Semaphore Wait when thread acquires lock
    //IntStatus oldLevel = interrupt->setLevel(IntOff); //Disable interrupts
    lockHolder = currentThread; //Lock is BUSY
    //interrupt->SetLevel(oldLevel);                    //Re-enable interrupts
}

//----------------------------------------------------------------------
// Lock::Release
// 	Allows any process to release the lock
//	after it is done accessing shared variables.
//----------------------------------------------------------------------
void Lock::Release()
{
    ASSERT(isHeldByCurrentThread()); //Evaluates if currentThread is lockHolder.
                                     //Terminates program if assertion fails.
    //IntStatus oldLevel = interrupt->setLevel(IntOff); //Disable interrupts
    lockHolder = NULL; //Lock is FREE
    //interrupt->SetLevel(oldLevel);                    //Re-enable interrupts
    mutex->V(); //Call Semaphore Signal when thread is done with lock
}

//----------------------------------------------------------------------
// Lock::isHeldByCurrentThread
// 	Assertion function to determine whether the process that
//  calls Release() is the same one that had invoked Acquire() previously.
//----------------------------------------------------------------------
bool Lock::isHeldByCurrentThread()
{
    bool held = false;
    if (lockHolder == currentThread)
    {
        held = true;
    }
    return held;
}

//----------------------------------------------------------------------
// Condition::Condition
// 	Initialize a condition variable, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//----------------------------------------------------------------------
Condition::Condition(const char *debugName)
{
    name = (char *)debugName; //for debugging
    waitQueue = new List<Semaphore *>;
}

//----------------------------------------------------------------------
// Condition::Condition
// 	De-allocate waitQueue, when no longer needed.
//----------------------------------------------------------------------
Condition::~Condition()
{
    delete waitQueue;
}

//----------------------------------------------------------------------
// Condition::Wait
// 	Releases the lock and gives up CPU until signalled
//	Re-acquires lock afterwards
//----------------------------------------------------------------------
void Condition::Wait(Lock *conditionLock)
{
    Semaphore *semaphore;
    ASSERT(conditionLock->isHeldByCurrentThread()); //Current thread must hold conditionLock
    semaphore = new Semaphore("condition", 0);      //Create new semaphore
    waitQueue->Append(semaphore);                   //Semaphore has been allocated to waiting thread
    conditionLock->Release();                       //Release lock atomically
    semaphore->P();                                 //Another thread wakes condition variable
    conditionLock->Acquire();                       //Re-acquire lock
    delete semaphore;
}

//----------------------------------------------------------------------
// Condition::Signal
// 	Wake up one sleeping thread
//	If there are any on the condition variable
//----------------------------------------------------------------------
void Condition::Signal(Lock *conditionLock)
{
    Semaphore *semaphore;
    ASSERT(conditionLock->isHeldByCurrentThread()); //Current thread must hold conditionLock
    if (!waitQueue->IsEmpty())
    {
        semaphore = waitQueue->Remove(); //Get thread and remove thread from waitQueue
        semaphore->V();                  //Wake up thread
    }
}

//----------------------------------------------------------------------
// Condition::Broadcast
// 	Wake up all sleeping threads waiting on the condition variable
//----------------------------------------------------------------------
void Condition::Broadcast(Lock *conditionLock)
{
    while (!waitQueue->IsEmpty())
    {
        Signal(conditionLock);
    }
}

