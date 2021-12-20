# B+ tree analysis

This document refers to an analysis about B+ Tree operations such as insert and delete.

## premises

First of all, the premises of B+ Tree are the followings.

1. All keys in single tree are unique, that is no duplicates for leaves' keys.
1. Keys are stored in ascending order in all nodes.

## call path of insertion

### overall flows

At first, when key and value are passed into `#insert(node* root, int key, int value)`, finding duplicates in existing nodes is proceeded by `find( node * root, int key, bool verbose )`. If there is no existing node in tree, create a new leaf node then insert the key and value. On the other hand, through `find_leaf( node * root, int key, bool verbose )`, find the candidate leaf node where the new record will be inserted.

After then, check a capacity of the candidate leaf node whether it will be able to contain new key and value without violating the rule which specifies the maximum number of records. Upon the checking the capacity, there are possible situations;

1. The leaf can obtain new value.
    * In this situation, simply insert into it.
1. The leaf cannot obtain new value.
    * This will lead special operation, "split".

### split operation

#### insert_into_leaf_after_splitting

Split operation, `insert_into_leaf_after_splitting(node * root, node * leaf, int key, record * pointer)`, contains the action that exceeding node of both internal and leaf is splitted with newly-created node in order to contain the new key and value.
To begin with, create arrays for keys and pointers that can contain not only existing ones, but also new one. After insert new record and sorting them, find the split point through `cut( int length )` that calculates the half of the keys in a node. Then create new node then insert the portion of keys and values indicated by the split point.
The next step is about created node. It contains also its own key and has to be referred by other internal nodes on upper layer. For that reason, `insert_into_parent(node * root, node * left, int key, node * right)` is called.

#### insert_into_parent

There is a duty that newly created node should be referred by its parent node. If there is no parent node, create new parent node and insert the new reference with `insert_into_new_root(node * left, int key, node * right)`. In other cases, take the leftmost key of the new node and try to insert into the parent. This step is exactly the same operation with checking capacity, which is performed before. If it is not able to insert, `insert_into_leaf_after_splitting(node * root, node * leaf, int key, record * pointer)` is called recursively.

## call path of deletion

### overall flows

For delete operation, `db_delete( node * root, int key )`, find the leaf containing the requested key by `find_leaf( node * root, int key, bool verbose )` and call `delete_entry( node * root, node * n, int key, void * pointer )`. After retrieving the record, release(free) allocated memory for that one.
To be specific, simply deleting the key and value at first and pack the empty holes in keys or values derived from the deletion. The remain tasks to perform is reorganize the node and its parent. However, it is not required for the case of root node since it doesn't generate propagations for others.
To deal with that tasks, determine the threshold of the node. There might be three following cases.

1. It is still above the threshold about free space.
    * **CASE1** no need to handle with this kind of nodes.
2. There is an excessive free space.
    1. neighbor node has enough space to absorb it.
    * **CASE2** neighbor node merge the remnant values of the node.
    2. neighbor node can't cover all its data.
    * **CASE3** the node pulls some keys and value from the neighbor.

**CASE2** and **CASE3** are described in following details.

### merge operation

In case of **CASE2**, `coalesce_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime)` method is called, then append key and values into neighbor node. The appending direction is determined upon whether the neighbor is left side or right side. move all keys and pointers of the victim into neighbor. Especially  for the case of internal node, update its children parent references. When finishing the shifts, delete the victim node. Then, in order of removing the victim's reference, modify its parent node by calling `delete_entry( node * root, node * n, int key, void * pointer )` recursively.

### redistribute operation

In case of **CASE3**, this triggers `redistribute_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime_index, int k_prime)` method. Along with a step of `merge operation`, the redistribute direction also should be determined considering the orders between node and its neighbor. Until the node's the amount of free space is under the threshold, pop the keys and poitners from its neighbor then append into itself. Then the key range of node about either node or neighbor is changed, it triggers that the reference from its parent node should be changed consequently. Compared to merge operation, the total number of nodes is not changed, so just simply alter old key with new one.

## changes for building on-disk B+ Tree

### Modify references through pointer

In in-memory B+ tree, each node refers others by the data type of `void ** pointers`. However, in the circumstance that all data is stored on disks, this should be changed as unique table id and page number which imply the correct destination in file system. Through this metadata of referencing other nodes, when a node has to access them, it should retrieve them in the first step.

### Add a write call after manipulations

When nodes are on in-memory, all changes of keys and pointers are applied directly into them. However, for on-disk situation, these modifications should be properly written down throw file system call. Otherwise, they will cause improperly reads in another operations.