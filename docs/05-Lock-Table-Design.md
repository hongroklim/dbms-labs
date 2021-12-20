# Lock Table Design

This document demonstrates the design of the lock table and how it works.

## Outline

To begin with, a thread requests `lock_acquire()` to lock table with `table_id`
and `key`. Then, the Lock table makes a key corresponding to the composition of
those parameters, and finds a matched record entry in its lock table.
If there is no existence of the purposed entry, it creates a new one.

With a found entry, the lock table check whether there is acquiring lock or not.
If the lock list is empty, return the lock object right away.
However, when it have to wait until the predecessors have done their tasks,
the new lock object is attached behind the existing locks as wating the fronts.

When it is turn to the lock object, `lock_acquire()` finally terminates giving
the output of the lock object to the calling thread. After the thread finishes
the jobs given the lock, it passes lock object through `lock_release()`.

## Hashing Algorithm

I've used `std::hash<uint64_t>` function to figure out the hash key of 
`table_id` and `key`. The parameter of hash function is the summation
of `table_id` and `key`. Since the ranges of both data types are smaller than
`uint64_t`, it is safe to use this algorithm while providing the safe hashing
of enormous inputs.

## Lock Table's Data Structure

The internal structure of the lock table is `std::map`. The reason why I have
chosen this data structure is that its keys are unique and it is implemented
with red-black trees, which guarantees the logarithmic complexity for 
operations.

## Resolving Collisions

When there are collisions that some of record entries have the same hash key.
To handle such a scenario, duplicated entries are linked from the lock table
in a single direction.

## Mutex Usage

There are two kinds of mutexes in the lock table as followings.

- Lock Table itself
- record entries

During the operations like finding or manipulating the lock table, the mutex
of the lock table is utilized. On the other hand, while changing a head or
a tail for each entry, its mutex is in the usage.

## Wake-up the descendent

After the foremost lock finishes the tasks, it informs another lock sleeping
behind through `pthread_cond_signal()`. When the later lock get the signal,
lock table returns it to the caller which is a thread.

There is a possible problem of the *lost wake-up* between caller and receiver.
To resolve this, after locking the mutex, it confirms whether the entry's header
is the same with itself or not. If the comparison matches, it skips the waiting
for the alert. These are the methods how I overcome the predicted matter.