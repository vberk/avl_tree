Implementation of an AVL tree using pointers, including rebalancing delete,
and memory management.  Binary trees have O(log n) performance.

Usage: objects are void* pointers and can be any type of structure.
The evaluation method must return <0 (smaller), 0 (match), or >0 (bigger),
when passed two void* data pointers.  The 'key' can therefore be a small
part of the structures to be stored and sorted.  The example evaluation
and print-callback methods are given where the data are simple int* pointers.

Memory management:  Nodes for the tree are allocated in blocks of 'N'
(preferably adapted to page size), and stored on a stack of free nodes.
Delete returns nodes to this stack.  If a tree shrinks substantially, the
de-alloc method can be used to return any free memory from the free stack.
Using an allocation size of N=128; pages of 4kb are allocated at once.

This tree is not re-entrant.  Any modification (meaning insert or delete)
must be exclusive.  Any non-modifying method can be concurrent (search/find,
walk/serialization, and print).  Use external locking.

Serializing the tree is best done through the 'walk' method, that calls
the callback for each object in the tree, in sorted order.  The actual
structure of the tree does not need to be serialized upon storage or
transmission, as it will rebuild automatically upon 'insert' calls.

Crafty use of the main tree structure allows multiple trees to be built
from the same allocation set.  It requires that the 'top', and
'height' to be managed externally.

Inserting, deletion, and rebalancing algorithms adapted from Knuth's art
of computer programming. (pg 458, volume 3, 3rd ed.)
