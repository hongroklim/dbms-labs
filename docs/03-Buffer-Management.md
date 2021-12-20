# Buffer Management

All IOs from above the buffer layer must be performed through this buffer layer.
This document describes how buffer manager works on DBMS and relationships with upper or lower layers.

About the explanation how I allocate and free pages, please refer to [this document](04-Allocation-and-Free-Strategy).

## Buffer Management Layer itself

To begin with, I made an `block_t` structure that implements the buffer block following the announced specs. The additional field is `block_id` that shows the unique number of buffer block in order to make it easy to debug this layer.

### Buffer Block Structure

* `int block_id` : Unique buffer block ID
* `page_t* page` : Frame to hold data
* `int64_t table_id`
* `pagenum_t pagenum`
* `bool is_dirty`
* `bool is_pinned`
* `block_t* prev`
* `block_t* next`

### Buffer Controller

Buffer Controller is dedicated managing the requests to allocate and flush frames.
Because the maximum number of frames is limited, it has responsibility to ensure
all reserved and not reserved frame buffers are handled correctly, and to execute
expected works from the upper layer.

#### Variables

* `int nextBufferId = 1` : Role as Auto-Increment or Sequence to generate next block ID
* `int bufferNumber = -1` : Keep the total number of buffer blocks
* `block_t* freeBlock` : Indicate the first free buffer block 
* `block_t* headBlock` : Indicate the first reserved buffer block
* `block_t* tailBlock` : Indicate the last reserved buffer block

### Empty Buffer Blocks

All empty buffer blocks are double-linked together using `prev` and `next`.
Buffer Controller keeps the linked list through `freeBlock` pointer that indicates the first one.
It acts like a stack, Last In First Out.

### LRU Strategy

This buffer manager adopts LRU (Least Recently Used) algorithm to manage reserved frames.
All frames are double-linked (prev & next) and there are external pointers to indicate
head and tail of list.

#### Usage of Head and Tail

When new data is inserted, it calculates the availability of empty frames.
When there is no empty, that is, the number of filled frames has already matches with the
configured total numbers, then the last *available* frame `(starts with tail)` becomes the eviction and would be flushed.
*Available* means that it isn't a pinned status.
Then the newly allocated frame is located the top of the list `(head)`.

## Relationships with Other Layers

I totally cut the relationships between `Index` and `Disk Space` layer.
So I guarantee that there is no `#include <Disk Space Manager>` annotaion in any files of index management.
Although I have noticed that according to piazza @191,
some methods like `open_table` and `close_table` can be called directly,
I just want to purely implement a layered-architecture.

### In the View of Index Management

The time when buffer block is pinned is that `buffer_alloc_page` or `buffer_read_page` are executed.
Before index management get requested data,
the corresponding buffer block has been already tagged as `is_pinned`.
Furthermore, when the page's data is changed through `buffer_write_page`,
the buffer block's information of dirty status will be changed.

I have implemented all pages not by `page_t`, but through `AbstractPage` class, that is,
I can successfully use the base class's destructor as the point of `buffer_unpin`.
After calling `delete` operation or reaching end of block, which means that the reserved page won't be used,
unpin method is executed simultaneously.

### In the View of Disk Space Management

Because buffer layer is the only referer that calls `Disk Space Management`, this layer doesn't have to care other layer's requests.