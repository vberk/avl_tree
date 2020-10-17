Implementation of an AVL tree using pointers, including rebalancing delete,
and memory management.  Binary trees have O(log n) performance.

Usage: objects are void* pointers and can be any type of structure.
The evaluation method must return -1 (smaller), 0 (match), or +1 (bigger),
when past two void* data pointers.  The 'key' can therefore be a small
part of the structures to be stored and sorted.  The example evaluation
and print-callback methods are given where the data are simple int* pointers.

Memory management:  Nodes for the tree are allocated in blocks of 'N'
(preferably adapted to page size), and stored on a stack of free nodes.
Delete returns nodes to this stack.  If a tree shrinks substantially, the
de-alloc method can be used to return free memory rom the free stack.
Using an allocation size of 128, pages of 4kb worth are allocated at once.

This tree is not re-entrant.  Any modification (meaning insert or delete)
must be exclusive.  Any non-modifying method can be concurrent (search/find,
walk/serialization, and print).  Use external locking.

Serializing the tree is best done through the 'walk' method, that calls
the callback for each object in the tree, in sorted order.  The actual
structure of the tree does not need to be serialized upon storage or
transmission, as it will rebuild automatically upon 'insert' calls.

Inserting, deletion, and rebalancing algorithms adapted from Knuth's art
of computer programming. (pg 458, volume 3, 3rd ed.)
