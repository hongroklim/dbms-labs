# Concurrency Control

This document explains about transaction and locking.
Before reading, I metion that this transaction manager implements
implicit locking and lock compression.

## Overview of Transaction Manager

The overall architecture of transaction manager includes `trx`, `LockContainer`,
`LockEntry`, and `TrxContainer`. `trx` has a role of mainstream on this service.
When a database is initializing, it creates other components and fully controls them.
`LockContainer` and `LockEntry` are needed to manage the lock table. When a `trx`
requires lock information about specific *table* and *page*, `LockContainer` will return
the `LockEntry` corresponding to the parameters of *table* and *page*. `TrxContainer`
contains the alive transaction ids and their locks connected through a linked list.
This class is also used to manage the list of implicit locks. *(It will be explained
again in the followings)*

## Structure of Lock

Before the detailed explanations of transaction manager, `struct lock_t` provides
you with an broad insight of this service. `lock_t` consists like the below.

```c
// Identifier
int64_t key{};
int lockMode{};

lock_t* prev{};
lock_t* next{};

LockEntry* sentinel{};

bool isAcquired{};
bool isReleased{};

// Transaction
lock_t* trxNext{};
int trxId = 0;

// Mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int waitCnt = 0;

// Lock Compression
bool* bitmap{};
std::map<int64_t, int>* keyIndexMap{};

// Logging
bool isDirty = false;
char* org_value{};
int org_val_size = 0;
```

**Identifier** refers the variables for representing the lock itself and
linking with others in the lock table. In addition, I added `isAcquired` and `isReleased`
in the purpose of deadlock detection and preventing the lost-wake up problem for each.

**Transaction** keeps the information of its transaction id and sibling locks of
its transaction. Through a `trxNext`, the linked list of locks in same
transactions can be managed.

**Mutex** signals the waiting locks if they exists. `waitingCnt` tracks how many
locks are waiting for this working lock.

**Lock Compression** is implemented with an array of bits. Its length is same
with the number of slots in the leaf page. `keyIndexMap` maps between the key and
index of the slot in itself.

**Logging** properties are temporary attributes that will be replaced in project 6.
Before the updates, it holds the original value and value size. Through these
variables, rollback operation will be executed if necessary.

## Buffer Manager Latch

Because this DBMS was developed as if there is only a single thread, other managers
outside of transaction manager should be modified properly. One of these changes
is a buffer manager.

In the circumstance of multi-thread operations, buffer should carefully flush and
drop while keeping the active pages. A thread should acquire two mutexes sequentially
to execute a `buffer_read_page` function; *Buffer Manager Latch* and *Block Latch*.
During the `buffer_pin` and `buffer_unpin`, it was initially implemented by a boolean
variable. However, at this moment, this is replaced with `mutex_lock` and `mutex_unlock`.
Considering a single thread can pin the same block at multiple times (i.e. Update the leaf
and its parent), the block level mutex supports `PTHREAD_MUTEX_RECURSIVE`.

## Transaction Manager Latch

When `trx_begin()` function is called, it generates an sequential number of 
transactions and mark it as alive. During the entire processes, it should be protected
with a mutex, that is transaction manager latch.

The list of active transactions also should be reguard as a single critical logic.
When it is guaranteed that a single thread can access, appending or dropping a
transactions are available. This latch is also applied when a new lock is appended
or pop out from the list.

## Lock Mode

There are two kinds of locks; `Shared` and `Exclusive`. `Shared` lock can be acquired
multiple times contemporarily. However, for a same record, `Exclusive` lock is a sole
and other locks should wait for this acquired lock. Upon the lock mode, here are
some examples whether the lock is acquired or waiting.

```text
S(acquired) <- S(acquired) <- S(acquired)
X(acquired) <- S(waiting)  <- S(waiting)
S(acquired) <- X(waiting)  <- S(waiting)
```

In this implementation, this property is distinguished by a `int lockMode` variable
with static constants; `LOCK_TYPE_SHARED` and `LOCK_TYPE_EXCLUSIVE`.

## Lock Acquire

Here's the explanation what happens during the `lock_acquire`.

* In the corresponding leaf page, get index of key among slots.
* unlock the buffer page.
* Retrive a `LockEntry` that contains the list of locks in that page.
* lock the entry.
* In the transaction list, Check whether this lock has been already acquired or not.
  * return the founded one if it exists.
* Create a new lock instance.
* Get the list of locks which are located ahead of the new one.
  * If there is no precedent, check for the implicit lock.
    * If the valid implicit lock exists, convert it into the explicit one.
    * Otherwise, become an implicit lock itself for the `Exclusive` one.
* Through the list of precedent, check whether there is a deadlock.
  * If exists, abort and rollback.
* If it is `Shared` lock, try to apply the lock compression.
* Append this lock in to the transaction list.
* Set the head or tail of the new lock if necessary.
* If there is no precedent, return the lock immediately.
* Before the waiting the signal, check the `isReleased` variable.
* If it get the signal, return the lock.
  * For the last signal, the receiving thread delete the previous lock.

During the procedures, they lock or unlock the mutexes. To prevent a deadlock,
all mutexes should be unlocked before to obtain a new lock. while checking a deadlock,
it can be acquired the lock of both `LockEntry` and `Transaction Manager` because
it is an exceptional case. 

## Deadlock Detection

`trx_dead_lock` function's parameter is `int trxId` and `vector<lock_t*> prevLocks`.
It calls itself recursively and the terminate condition is that `prevLocks` are empty.
If the encountering lock has the same transaction id with `trxId`, it returns true.
Otherwise, among the locks in the encountering transaction, filter them which is
waiting and get list of previous locks for each. Then it calls `trx_dead_lock` again.
If there is no return of true, it will return false.

## Commit and Rollback

In the release phase, the explicit locks should signal the waiting locks, while
the implicit ones don't have to do. Then the obsolete lock should be popped out
from the linked list of lock table. After all locks are deleted, the key of
transaction manager will be removed. 

In the case of `Rollback`, revert the changes through the `org_val` and
`org_val_size` recorded inside of the lock structure. In the perspective of *Index
Manager*, this is performed when there is an exception or a failure to acquire the lock.

## Implicit Locking

During the lock acquiring process, when it noticed that there is no explicit lock
ahead of a new `Exclusive` one, it tries to make an implicit lock with `lock_implicit`
function. In this method, it accesses to the record in the buffer then check
whether there is a transaction id and it is still alive. If the new lock is `Exclusive`
and there is no valid implicit lock, it marks its own transaction id into the record.
However, the new lock should wait for the termination of implicit lock, it converts
the implicit lock into explicit one through the `lock_to_explicit` function.

## Lock Compression

If the new lock can be acquired immediately, the marks indicating that the record
is already occupied can be placed on the boolean list of existing lock object.
It is applied with the `Shared` lock because immediately acquirable `Exclusive`
lock is recorded through an implicit lock. While comparing the keys for the lock
structure, `lock_contains` method can be used. This function compares not only 
the `key` attribute, but `bitmap` indexed through a `keyIndexMap` mapping.
It should be reminded that lock compression technique can be applied for the
immediately acquirable lock. If the new lock should wait for the precedent, the
additional lock struct will be created.