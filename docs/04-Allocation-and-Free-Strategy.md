# Allocation and Free Strategy

This is a description how buffer management performs to allocate and free pages
While considering both buffer and disk space layer.

## Challenges

Above buffer layer, if DBMS needs to allocate or de-allocate a page, `buffer_alloc_page` or `buffer_free_page` will be called for each. There is a wierd situation on simple implication. That is, although lower layer *(disk space)* requires a header page directly, the cached data for a header page is reserved on upper layer *(buffer)*.

In short, I figured out the challenges in complying the specifications in followings.

1. Preserve the function signatures in each layer
1. Follow layered architecture as much as possible

In order of dealing with this matter, I applied two tricky techniques(or designs), `Function Overload` and `Dependency Injection`.

## Overloading (De)Allocation Methods

According to the specifications for a disk space management, `file_alloc_page` and `file_free_page` are fixed signatures,
which means that these can't be changed. Both methods invoke system calls(IOs) of header page,
and must be avoided from buffers since the caller has already contained the header page's data.

So I overloaded `file_alloc_page` and `file_free_page` methods by adding new parameter.
If those functions are called without new parameter, it will assign the default parameter to perform as if there are only system calls.
However, when new parameter is assigned, reading and writing the header page refer to data from *either file system or buffer*.

## Polymorphism of Header Source

I created `AHeaderSource` that is an abstract class to act like a storage of header page.
It has two methods; `read(page_t*)` and `write(const page_t*)`.
There are two derived classes `FileHeaderSource` and `BufferHeaderSource`.
As you can distinguish the signature, one class manipulates the page with file system, other deals with data
originated from the buffer block.

## Dependency Injection of HeaderSource

To explain the difference between `FileHeaderSource` and `BufferHeaderSource` for disk space management,
Let's assume the situation to write a header page. When `FileHeaderSource` is injected, the changed data will
be applied in a real database file through a system call. On the other hand, if there is `BufferHeaderSource` in the parameter,
the buffer block will contain the revision of header page.

## My Humble Opinion

While there are controversial issues about how to allocate or free the page,
I would firmly argue that my own design is fit to the specifications for the following reasons.

### Preserve the function signatures

I have maintained the signatures in disk space management by *overloading* them.
My changes won't conflict with the previous requirements and will fulfill the further specifications.

### Follow layered architecture's intensions

I preserved layered architecture enough through `Dependency Injection`.
From the standpoint of disk space manager, allocation and free methods can't distinguish between *file-driven* header page
and *buffer-driven* one. That is, when the functions are called by a buffer management,
disk space layer doesn't make up-call and also isn't coupled with upper layer since it deals with an *abstract methods*.

To be honest, I need instructor's feedbacks whether my strategy can be accepted or not.
If I am wrong, please figure out the flaws in my own design. Thank you!