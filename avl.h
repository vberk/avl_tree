/*
 *  Copyright (c) 2020 by Vincent H. Berk
 *  All rights reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met: 
 * 
 *  1. Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer. 
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution. 
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


/*
 *  Implementation of an AVL tree using pointers, including rebalancing delete,
 *  and memory management.  Binary trees have O(log n) performance.
 *
 *  Usage: objects are void* pointers and can be any type of structure.
 *  The evaluation method must return -1 (smaller), 0 (match), or +1 (bigger),
 *  when past two void* data pointers.  The 'key' can therefore be a small
 *  part of the structures to be stored and sorted.  The example evaluation
 *  and print-callback methods are given where the data are simple int* pointers.
 *
 *  Memory management:  Nodes for the tree are allocated in blocks of 'N'
 *  (preferably adapted to page size), and stored on a stack of free nodes.
 *  Delete returns nodes to this stack.  If a tree shrinks substantially, the
 *  de-alloc method can be used to return free memory rom the free stack.
 *  Using an allocation size of 128, pages of 4kb worth are allocated at once.
 *
 *  This tree is not re-entrant.  Any modification (meaning insert or delete)
 *  must be exclusive.  Any non-modifying method can be concurrent (search/find,
 *  walk/serialization, and print).  Use external locking.
 *
 *  Serializing the tree is best done through the 'walk' method, that calls
 *  the callback for each object in the tree, in sorted order.  The actual
 *  structure of the tree does not need to be serialized upon storage or
 *  transmission, as it will rebuild automatically upon 'insert' calls.
 *
 *  Inserting, deletion, and rebalancing algorithms adapted from Knuth's art
 *  of computer programming. (pg 458, volume 3, 3rd ed.)
 *
 */




#ifndef _AVL_TREE_H
#define _AVL_TREE_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


//  This maximum depth is defined to build static arrays of paths in the
//  trees.  It is not actually a physical limit of any kind, although a tree
//  of a depth of 64 nodes would need to be 2^(64+5) bytes just for AVL_NODEs...
#define AVL_MAX_DEPTH 64


//  32 bytes on a 64 bit system, 16 bytes on 32-bit system
typedef struct AVL_NODE_S
{
    struct AVL_NODE_S *l, *r;   //  Left and right sub-trees
    int8_t f;                  //  Flags:  balance, free/used, alloc (1st in sequence), free-me, and count (if 1st in sequence)
    void *d;                    //  The user data pointer.
}
AVL_NODE;


//  Global tree structure:
typedef struct
{
    //  The list of free nodes:
    struct AVL_NODE_S *freeStack;        //  freeStack->(*n).r->(*n).r->...->NULL
    int allocAtOnce;

    //  The tree and all:
    struct AVL_NODE_S *top;
    int height;     //  Height to the deepest node
    int size;       //  Number of nodes in the tree

    //  The method by which *data pointers are compared
    int (*eval)(void *d1, void *d2, void *user);
    void *user;
}
AVL_TREE;



//
//  Example comparator method, the *data pointers are in this
//  case assumed to be simple *int pointers.  The number returned
//  must be negative for smaller, 0 for same (item is identical
//  and therefore found in the tree), or positive for larger.
//  
//  On insert, find, delete, pointer *p is given to this evaluation
//  method for nodes stored along the search path.  This method is
//  also called on rebalancing, therefore all operations should use
//  the same data structure, even if the key is only part of that
//  data structure.
//
//  Search depth can be tracked as part of the 'k' pointer that
//  is given as the seach key.  Increment on each 'eval' call.
//
//  The 'user' pointer is passed as-is from creation time.
//
int AVL_exampleEval(void *data1, void *data2, void *user)
{
    return((*((int*)data2))-(*((int*)data1)));
}



/************************************************************************
 *                                                                      *
 *   Memory management                                                  *
 *                                                                      *
 ************************************************************************/


//
//  Each binary tree needs an evaluation method to decide how
//  to sort the data 'd1' vs. 'd2'.  An example evaluation method
//  is given above, for integers.  The data pointers may be complex
//  structures of which the 'eval' method only uses a small portion.
//
//  The 'user' pointer is stored in the tee, and returned to each
//  call to 'eval', to additional information can be passed.
//
//  'allocAtOnce' indicates how many nodes should be 'malloc-ed'
//  at a time.  Small numbers lead to overhead, big numbers lead
//  to wasted memory.  On a 64-bit system, allocating 128 nodes is
//  a exactly a 4kb page, and therefore a good number.
//
AVL_TREE *AVL_newTree(int allocAtOnce, int (*eval)(void *d1, void *d2, void *user), void *user);

//  
//  Break down the tree, and return all nodes to the 'free' stack:
//  The tree will be empty after this call, but memory is still allocated.
//
void AVL_flush(AVL_TREE *t);

//  
//  Check if any memory blocks (of 'allocAtOnce' size) are unused
//  and can be returned to the OS.  This method is CPU intesive, and
//  only makes sense when the tree has significantly shrunk through
//  deletes, or a 'flush' has recently been called.
//
//  Will NOT touch or harm nodes that are currently in the tree.
//
int AVL_dealloc(AVL_TREE *t);

//
//  Simply destroys the tree, and 't' cannot be used again after.
//  Calls 'flush', then 'dealloc', then 'free' on 't'.
//
void AVL_destroy(AVL_TREE *t);





/************************************************************************
 *                                                                      *
 *   Tree operations                                                    *
 *                                                                      *
 ************************************************************************/


//
//  Finding an item, based on a key, 'k'
//  Returns pointer 'p' if found.
//
//  Note, that, 'k' is passed directly to the 'eval' method
//  as data pointer 'd2'.  It is required to make sure that 'k'
//  is of the same datatype as the pointers 'd' passed
//  on insertion.
//
void *AVL_find(AVL_TREE *t, void *k);

//
//  Insertion of a new data element.
//  Returns:
//    0  on success
//    1  already in tree
//    2  unable to allocate memory
//
int AVL_insert(AVL_TREE *t, void *d);


//
//  Deletion.
//  Rebalances the tree after deleting a node.
//  Returns:
//    NULL - key 'k' was not found in the tree.
//    data - the data pointer originally given upon insert.
//
void *AVL_delete(AVL_TREE *t, void *k);



/************************************************************************
 *                                                                      *
 *   Printing and validation                                            *
 *                                                                      *
 ************************************************************************/


//
//  For a tree where the data are simple integers, this example
//  prints the sorted list of integers when AVL_walk is called.
//
void AVL_exampleCallback(void *d, void *user)
{
    fprintf(stdout, "%i", *((int*)d));
}


//
//  Method that walks the tree left-root-right.  (In sorted order).
//  Callback is called for each node in the sorted order of the nodes.
//
//  This method is handy for:
//    1) serializing the tree
//    2) rebuilding/re-allocating
//
void AVL_walk(AVL_TREE *t, void (*callback)(void *d, void *user), void *user);



//
//  Example method for printing to a stream a label for the node.
//  This example method assumes a simple list of integers was inserted.
//  For use with the 'AVL_print' method.
//
void AVL_examplePrintLabel(FILE *stream, void *d)
{
    fprintf(stream, "%i", *((int*)d));
}


//  
//  Print the tree to 'stdout' SVG with HTML header.
//  The output from this method can be saved directly to an .html
//  file for rendering the tree in a browser.  This is practical
//  only for trees up to 200 to 300 nodes, max.
//
void AVL_print(AVL_TREE *t, int x, int y, void (*printLabel)(FILE *stream, void *d));


//
//  Regression testing method.  Returns -1 if there's a balance or
//  height error in the tree, printing an error message to 'stderr'.
//  Call with:
//     AVL_checkBalance((*tree).top);
//
int AVL_checkBalance(AVL_NODE *n);





#endif







