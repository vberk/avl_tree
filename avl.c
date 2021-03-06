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



#include "avl.h"


//  
//  These methods operate on the flag field 'f'.
//
//  A note on balance:  balance is stored as a positive number, and should never be more
//  than -1, 0, or +1.  However, during deletion operations, it may become -2 or +2, in
//  which case additional rotations are necesary to bring the tree back into a -1,0,+1 state.
//  The upper nibble holds the 3 balance bits, lower nibble is the proper flags field.
//
//  Bits:     Mask:       Field:
//    7         0x80         unused (technically the 'sign' bit, which messes with shifting)
//    6-4       0x70         Balance+2 (to avoid shifting signed), must be -2, -1, 0, +1, +2  (maps to 0, 1, 2, 3, 4)
//    3         0x08         1st in alloc (base address of allocation, used for free())
//    2         0x04         Currently free vs. used
//    1         0x02         Cleanup: marked for freeing up y/n
//    0         0x01         Available

#define AVL_FLG_BAL     0x70        //  Used as a mask, upper nibble.
#define AVL_FLG_1ST     3           //  Used as a shift amount.
#define AVL_FLG_USD     2           //  idem
#define AVL_FLG_CLN     1           //  idem

//  Balance specific:
#define AVL_getbal(f)       (((int8_t)((f&AVL_FLG_BAL)>>4))-2)
#define AVL_setbal(f,b)     f=((f&(~AVL_FLG_BAL))|((((int8_t)b)+2)<<4))
#define AVL_incbal(f)       (f+=(int8_t)0x10)
#define AVL_decbal(f)       (f-=(int8_t)0x10)

//  Genefic for flags:
#define AVL_getbit(f,b)     (f&(((int8_t)0x1<<b)))
#define AVL_setbit(f,b)     f|=((int8_t)0x1<<b)
#define AVL_clrbit(f,b)     f&=(~(((int8_t)0x1)<<b))







/************************************************************************
 *                                                                      *
 *   Memory management                                                  *
 *                                                                      *
 ************************************************************************/



//
//  Building a binary tree requires at the very least an evaluation function
//  that compares the data pointers of two nodes.  The 'user' point is optional
//  in case additional data is needed in the evaluation function.
//
AVL_TREE *AVL_newTree(int allocAtOnce, int (*eval)(void *d1, void *d2, void *user), void *user)
{
    AVL_TREE *t=(AVL_TREE*)malloc(sizeof(AVL_TREE));
    if (t)
    {
        memset(t, 0, sizeof(AVL_TREE));
        if (allocAtOnce<1) allocAtOnce=1;
        (*t).allocAtOnce=allocAtOnce;
        (*t).eval=eval;
        (*t).user=user;
    }
    return(t);
}

//
//  Internal method to get a new node.  Sometimes we need a new node
//  and there are none, and we need to allocate a pile.
//  Note:  this is an internal method, and always expects 't' to exist
//
AVL_NODE *AVL_newNode(AVL_TREE *t)
{
    AVL_NODE *n;
    if ((*t).freeStack==NULL)
    { 
        //  Allocation:
        int i;
        n=(AVL_NODE*)malloc((*t).allocAtOnce*sizeof(AVL_NODE));
        if (n)
        {
            AVL_NODE *f=n;
            //fprintf(stderr, "ALLOC: %llx\n", (long long int) n);
            //  Push them onto the stack.
            for (i=0; i<(*t).allocAtOnce; i+=1)
            {
                n[i].f=0;
                n[i].r=(*t).freeStack;
                (*t).freeStack=&(n[i]);
            }
            //  Mark which one was first
            //  This is the allocation address of the sequence.
            AVL_setbit((*f).f,AVL_FLG_1ST);
        }
    } 
    
    //  Note:  if allocation failed, n will be NULL 
    n=(*t).freeStack;
    if (n)
    {
        (*t).freeStack=(*n).r;
        (*n).l=NULL;
        (*n).r=NULL;
        (*n).d=NULL;
        AVL_setbal((*n).f,0);
        AVL_setbit((*n).f,AVL_FLG_USD);
        (*t).size+=1;
    }
    return(n);
}


//
//  Breaks down the tree and returns all of them to the freeStack:
//
void AVL_flush(AVL_TREE *t)
{
    AVL_NODE *n;
    AVL_NODE *stack[AVL_MAX_DEPTH];
    int top;

        //  Obvious empty tree case:
    if ((*t).top==NULL) return;

        //  Top goes on the stack:
    n=(*t).top;
    stack[0]=n;
    top=1;
    while (top>0)
    {
        if ((*n).l)
        {
            //  If there's a left, cut it, recurse left.
            stack[top]=n;
            top+=1;
            n=(*n).l;
        }
        else if ((*n).r)
        {
            //  If there's no more left, cut it, recurse right.
            stack[top]=n;
            top+=1;
            n=(*n).r;
        }
        else
        {
            //  There is no left, and no right.
            //  Delete the node, and pop from stack:
            top-=1;
            AVL_NODE *m=stack[top];
            //  Did we come from left or right?
            if ((*m).l==n)
                (*m).l=NULL;
            else if ((*m).r==n)
                (*m).r=NULL;
            (*n).r=(*t).freeStack;
            (*t).freeStack=n;
            AVL_clrbit((*n).f, AVL_FLG_USD);
            n=m;
        }
    }
    (*t).top=NULL;
    (*t).height=0;
    (*t).size=0;
    return;
}


//
//  Runs the free list twice and sees how much can be cleaned up.
//  Returns the number of records freed.
//
//  NOTE:  this method is NOT re-entreable
//
int AVL_dealloc(AVL_TREE *t)
{
    int c=0;
    AVL_NODE *n=(*t).freeStack;
    AVL_NODE *l=NULL;               //  The 'to-be-freed' list, using (*n).l
    
        //  
        //  Note:  there is a special case where (*top)=NULL and we
        //  only need to 'collect' the AVL_FLG_1ST nodes for calling 'free'
        //
    while (n)
    {
        //  Is this one of a sequence?
        if (AVL_getbit((*n).f, AVL_FLG_1ST))
        {
            int i, j;

                //
                //  Step 1:  check if all of the nodes in this sequence
                //  can be de-allocated (they're not in the tree).
                //
            j=0;
            for (i=0; i<(*t).allocAtOnce; i+=1)
            {
                //  If this node is NOT in the tree, set a bit
                //  indicating it can potentially be freed.
                if (AVL_getbit(n[i].f, AVL_FLG_USD)==0)
                {
                    j+=1;
                    AVL_setbit(n[i].f, AVL_FLG_CLN);
                }
                else
                    i=(*t).allocAtOnce;  // Bail
            }
            //  Can all be freed?
            if (j==(*t).allocAtOnce)
            {
                //  Schedule this entire sequence for cleansing.
                //  It leaves the 'cln' bit set, and sticks the 1st
                //  allocated node 'n' in the free list, using the 'left'
                (*n).l=l;
                l=n;
                c+=(*t).allocAtOnce;
            }
            else
            {
                for (i=0; i<(*t).allocAtOnce; i+=1)
                    AVL_clrbit(n[i].f, AVL_FLG_CLN);
            }
        }
        //  Next one:
        n=(*n).r;
    }

        //
        //  IF there's any that can be cleaned, run the list again, and
        //  take all the nodes out:
        //
    if (l)
    {
        //  Start by taking the nodes of each sequence out:
        AVL_NODE *p=NULL;
        n=(*t).freeStack;
        while (n)
        {
            if (AVL_getbit((*n).f, AVL_FLG_CLN))
            {
                if (p)
                    (*p).r=(*n).r;
                else
                    (*t).freeStack=(*n).r;
            }
            else
            {
                //  Go to the next one:
                p=n;
            }
            n=(*n).r;
        }
        //  Now run the free's:
        while (l)
        {
            AVL_NODE *q=l;
            l=(*l).l;
            //fprintf(stderr, "DE-ALLOC: %llx\n", (long long int) q);
            free(q);
        }
    }
    //fprintf(stderr, "Top: %llx   freeStack:  %llx\n", (*t).top, (*t).freeStack);

    return(c);
}



//
//  Flush the tree, de-alloc, and done.
//
void AVL_destroy(AVL_TREE *t)
{
    AVL_flush(t);
    AVL_dealloc(t);
    free(t);
    return;
}




/************************************************************************
 *                                                                      *
 *   Tree operations                                                    *
 *                                                                      *
 ************************************************************************/



//
//  Finding an item, based on a key, 'k'
//  Returns pointer 'p' if found.
//
void *AVL_find(AVL_TREE *t, void *k)
{
    void *d=NULL;
    AVL_NODE *c=(*t).top;

    while (d==NULL && c!=NULL)
    {
        int e=(*t).eval((*c).d, k, (*t).user);
        if (e==0)
        {
            d=(*c).d;
            c=NULL;
        }
        else if (e<0)
        {
            //  Left
            c=(*c).l;
        }
        else
        {
            //  Right
            c=(*c).r;
        }
    }
    return(d);
}


//
//  Insertion (vol 3, pg 462, 3rd ed.)
//  RC:
//    0  on success
//    1  already in tree
//    2  unable to allocate memory
//
int AVL_insert(AVL_TREE *t, void *d)
{
    int rc=-1;
    AVL_NODE *c=(*t).top;   //  Current node we're working on  (P)
    AVL_NODE *b=(*t).top;   //  Balance node  (S)
    AVL_NODE *p=NULL;       //  Parent of balance node  (T)
    AVL_NODE *n=NULL;       //  The new node, if it was added.  (Q)


        //  Simplest case is the tree is empty:
    if (c==NULL)
    {
        c=AVL_newNode(t);
        if (c)
        {
            (*c).d=d;
            (*t).top=c;
            (*t).height=1;
            rc=0;
        }
        else
            rc=2;
    }

        //  If the tree is not empty, search for a spot
        //  (Note that rc!=-1 if the tree was empty)
    while (rc==-1/* && c!=NULL*/)
    {
        //  Compare (A2)
        int e=(*t).eval((*c).d, d, (*t).user);
        if (e==0)
        {
            rc=1;
        }
        else if (e<0)
        {
            //  Left (A3):  if there's a node, we traverse down
            //  If there's nothing there, we add:
            if ((*c).l)
            {
                //  Move left, record the balance info
                if (AVL_getbal((*(*c).l).f)!=0)
                {
                    b=(*c).l;
                    p=c;
                }
                c=(*c).l;
            }
            else
            {
                //  Add a node to the left. (A5)
                n=AVL_newNode(t);
                if (n)
                {
                    //  Successfully added:
                    (*n).d=d;
                    (*c).l=n;
                    rc=0;
                }
                else
                    rc=2;
            }
        }
        else
        {
            //  Right (A4):  same deal.
            if ((*c).r)
            {
                //  Move right, record the balance info
                if (AVL_getbal((*(*c).r).f)!=0)
                {
                    b=(*c).r;
                    p=c;
                }
                c=(*c).r;
            }
            else
            {
                //  Add a node to the right. (A5)
                n=AVL_newNode(t);
                if (n)
                {
                    //  Successfully added:
                    (*n).d=d;
                    (*c).r=n;
                    rc=0;
                }
                else
                    rc=2;
            }
        }
    }

        //  If a new node was added, now the balance
        //  must be checked and corrected
    if (n)
    {
        AVL_NODE *r=NULL;       //  Rebalance point (R)
        int a;                  //  Off-balance angle
        int e;

        // Setting the balance factors (A6)
        e=(*t).eval((*b).d, d, (*t).user);
        if (e<0) 
        {
            a=-1;
            r=(*b).l;
        }
        else
        {
            a=1;
            r=(*b).r;
        }
        c=r;

        //  Run down from the rotate node to the new one 'n',
        //  updating all balances (currently all 'in-balance'):
        while (c!=n)
        {
            //  These duplicate prior eval calls, unfortunately:
            //  (Optionally, these evaluations could be made 16 bit and
            //   temporarily stored in the free bits in AVL_NODE)
            e=(*t).eval((*c).d, d, (*t).user);
            if (e<0)
            {
                AVL_setbal((*c).f,-1);
                c=(*c).l;
            }
            else
            {
                AVL_setbal((*c).f,+1);
                c=(*c).r;
            }
        }

        //  Test the condition of the tree:
        if (AVL_getbal((*b).f)==0)
        {
            //  (A7.i) Tree is now 1 level deeper/higher
            AVL_setbal((*b).f,a);
            (*t).height+=1;
            //  Done.
        }
        else if (AVL_getbal((*b).f)==-a)
        {
            //  (A7.ii) Now the tree is more balanced:
            AVL_setbal((*b).f,0);
            //  Done.
        }
        else  //  (AVL_getbal((*b).f)==a)
        {
            //  (A7.iii) Rebalancing is required:
            if (AVL_getbal((*r).f)==a)
            {
                //  This is a single rotation (A8)
                c=r;
                if (a==-1)
                {
                    (*b).l=(*r).r;
                    (*r).r=b;
                }
                else
                {
                    (*b).r=(*r).l;
                    (*r).l=b;
                }

                //  Balances:
                AVL_setbal((*b).f,0);
                AVL_setbal((*r).f,0);
            }
            else    //  balance of rotate node is -a
            {
                //  This is the double rotation (A9)
                if (a==-1)
                {
                    c=(*r).r;
                    (*r).r=(*c).l;
                    (*c).l=r;
                    (*b).l=(*c).r;
                    (*c).r=b;
                }
                else
                {
                    c=(*r).l;
                    (*r).l=(*c).r;
                    (*c).r=r;
                    (*b).r=(*c).l;
                    (*c).l=b;
                }

                //  Balances:
                if (AVL_getbal((*c).f)==a)
                {
                    AVL_setbal((*b).f,-a);
                    AVL_setbal((*r).f,0);
                }
                else if (AVL_getbal((*c).f)==0)
                {
                    AVL_setbal((*b).f,0);
                    AVL_setbal((*r).f,0);
                }
                else  //  AVL_getbal((*c).f)==-a
                {
                    AVL_setbal((*b).f,0);
                    AVL_setbal((*r).f,a);
                }
                AVL_setbal((*c).f,0);
             }

            //  Finally, touch up the top of the tree (A10)
            if (p)
            {
                if ((*p).l==b)
                    (*p).l=c;
                else
                    (*p).r=c;
            }
            else
            {
                (*t).top=c;
            }
        }
    }

    return(rc);
}



//
//  Deletion.
//  Rebalances the tree after deleting a node.
//  Returns:
//    NULL - node was not found in the tree.
//    data - the data pointer originally given upon insert.
//
//  Notes on general algorithm:
//   Find the node that needs to be deleted, if it has 2 children, then  
//   recurse down and find the closest in-order successor/predecessor.  This is
//   necessarily a node all the way down, but _might_ have a single child. 
//   Swap this node from the bottom of the tree into the slot of the node that
//   is about to be deleted.
//   At this point an inbalance may have occurred, that needs to be rectified.
//   Unfortunately, it might completely unbalance the tree to the root, and
//   all balances must be checked, rotating until the root is balanced.  Rotations
//   are analogous to the insertion case.
//   Identical to the 'walk' method below a stack is kept of the path in the
//   tree from the root to the node to be deleted.
//   Delete returns the 'data' pointer '(*n).d', or NULL if not found.
//   
void *AVL_delete(AVL_TREE *t, void *k)
{
    AVL_NODE *c, *p;
    AVL_NODE *stack[AVL_MAX_DEPTH];
    void *d;
    int top;
    int h=0;        //  Tracks if the tree is getting shorter.

    //  No root, no need:
    if ((*t).top==NULL)
        return(NULL);


    //
    //  Record the path to the node to be deleted:
    //
    top=0;
    c=(*t).top;
    d=NULL;     //  The 'data' pointer
    p=NULL;     //  Parent

    while (d==NULL && c!=NULL && top<AVL_MAX_DEPTH)
    {
        int e;

        //  This way the last item on the stack can never be NULL
        //  'top' is only updated in case e!=0, so that the deleted
        //  node 'c' is technically never top-of-stack.
        stack[top]=c;

        //  Left, right, or found.
        e=(*t).eval((*c).d, k, (*t).user);
        if (e==0)
            d=(*c).d;
        else if (e<0)
        {
            p=c;
            c=(*c).l;
            top+=1;
        }
        else
        {
            p=c;
            c=(*c).r;
            top+=1;
        }
    }

    //  At this point, if no 'd' was found, the item is not in the tree:
    if (d==NULL)
        return(d);

    //
    //  At this point, 'c' points to the node that is to be deleted.
    //  If there is only a single subtree, replace 'c' with that tree,
    //  no replacement node needs to be found:
    //
    if ((*c).l==NULL || (*c).r==NULL)
    {
        AVL_NODE *s=(*c).l;     //  Left subtree.
        if (s==NULL)
            s=(*c).r;           //  Right subtree.

        //  If there is a previous
        if (p)
        {
            //  Was 'c' the left or right child?
            if ((*p).l==c)
            {
                //  The tree is getting shorter on the left:
                (*p).l=s;
                AVL_incbal((*p).f);
            }
            else
            {
                //  The tree is getting shorter on the right:
                (*p).r=s;
                AVL_decbal((*p).f);
            }
            //  Important, did the total tree get shorter?
            //  This happens when the tree is now balanced (from -1 or +1):
            if (AVL_getbal((*p).f)==0)
                h=-1;
        }
        else
        {
            //  'c' is the top
            //  Remember that the tree has become less tall, but
            //  the balance under 's' is already correct:
            (*t).top=s;
            h=-1;
        }
    }
    else
    {
        //
        //  Find the in-order predecessor or successor.
        //  Make the decision based on where the balance sits, so
        //  the tree becomes 'more' balanced, potentially avoiding
        //  additional balancing rotations.
        //  
        //  The process is straight forward: if the balance is positive
        //  ((*c).r is deeper), then go right once, and continue going left
        //  until there is no further left.  At this point the in-order
        //  successor has been found, but might have a right subnode still.
        //
        int cpos=top;       //  Remember where in the stack 'c' is located
        AVL_NODE *sc=c;     //  This is the actual node we're going to delete, saved.
        AVL_NODE *sp=p;     //  And the parent, also saved.
        AVL_NODE *s=NULL;   //  The in-order neighbor may still have a subtree on the other side.

        //  'c' is already saved at 'stack[top]', move one up:
        top+=1;
        p=c;

        if (AVL_getbal((*c).f)>0)
        {
            //  Right-hand side is taller, grab the in-order successor
            c=(*c).r;
            while ((*c).l && top<AVL_MAX_DEPTH)
            {
                stack[top]=c;
                p=c;
                c=(*c).l;
                top+=1;
            }
            //  Save potentially a subtree still on the right:
            s=(*c).r;
        }
        else
        {
            //  Left-hand side is taller, grab the in-order precursor
            c=(*c).l;
            while ((*c).r && top<AVL_MAX_DEPTH)
            {
                stack[top]=c;
                p=c;
                c=(*c).r;
                top+=1;
            }
            //  Save potentially a subtree still on the left:
            s=(*c).l;
        }

        //  At this point 'c' points to the inorder sucessor/precursor, and is
        //  the node that is going to be put in position of 'sc'.  Any eventual
        //  subtrees (in 's') can be put in place of 'c', carving 'c' out of the
        //  tree.  Now 'p' again points to the parent of 'c', and needs a balance
        //  update:  specifically, if 'c' was on the left of the parent, then the
        //  balance is now more to the right, so it is incremented, and vice versa.
        //
        //  In this case, 'p' always exists, because the case where the deleted node
        //  was at the top of the tree was already handled above.  It is possible that
        //  'p' is pointing at 'sc', which is not a problem.  Note that 's' is 50/50
        //  change of being NULL, which does not matter.
        if ((*p).l==c)
        {
            (*p).l=s;
            AVL_incbal((*p).f);   //  Tree under 'p' got shorter on the left
        }
        else
        {
            (*p).r=s;
            AVL_decbal((*p).f);   //  Tree under 'p' got shorter on the right
        }
        //  If 'p' became 'more' balanced (ie. went to 0), then the tree actually
        //  got shorter:
        if (AVL_getbal((*p).f)==0)
            h=-1;

        //  'c' is going to be swapped into place of 'sc', before 'sc' is discarded.
        //  The balance is copied, and the pointers connected.
        (*c).l=(*sc).l;
        (*c).r=(*sc).r;
        AVL_setbal((*c).f, AVL_getbal((*sc).f));
        //  Parent connections:
        if (sp==NULL)
            (*t).top=c;
        else if ((*sp).l==sc)
            (*sp).l=c;
        else
            (*sp).r=c;
        //  Ensure that 'c' is in the path instead of 'sc':
        stack[cpos]=c;

        //  Make sure that 'c' (which is to be deleted), is now again correctly
        //  pointing to the node that was taken out of the tree, ie. 'sc'
        c=sc;
    }


    //
    //  At this point 'c' is out of the tree, and 'd' has been saved.
    //
    AVL_clrbit((*c).f, AVL_FLG_USD);
    (*c).d=NULL;
    (*c).l=NULL;
    (*c).r=(*t).freeStack;
    (*t).freeStack=c;
    (*t).size-=1;
    c=NULL;

    //
    //  Rebalance:
    //   The position in the stack points to either: 1) the node that was taken
    //   and placed in the position of the deleted node, or 2) the deleted node
    //   itself.  So balance must be checked pos-1 and up to the root.
    //
    //   Since we allow the blance to become +2 or -2 above, we must perform
    //   rotations.  Depending on whether it is left or right side off-balance
    //   the process is identical, just mirrored.  There's two possible scenarios
    //   for the node 'a' under consideration (balance in braces).  
    //
    //   Examples given for the positive balance (right-side 'heavy') case only.
    //   Nodes 'a', 'b', 'c'   are going to get rotaded around eachother.  Node 'p'
    //   is always the parent of the subtree under rotation, and 's1-4' are subtrees
    //   that are untouched (possibly 'NULL').
    //
    //
    //              a  (2)                              b
    //             / \                                 / \
    //           s1   b  (0,1)                       a     c
    //               / \                -->         / \   / \
    //             s2   c                         s1  s2 s3  s4
    //                 / \
    //               s3   s4
    //
    //   
    //     Cases:
    //       bal(b)=0       (s2)=h(c)=h(s1)+1  (and h(s2)=h(s3)+1 or h(s2)=h(s3)+1 (or both))
    //                        therefore:  bal(a)=+1  bal(b)=-1  bal(c)=unchanged, height(p)=unchanged (because s2 is +1 taller)
    //       bal(b)=+1      h(s2)=h(s3) or h(s2)=h(s4) (or both)  and h(s2)=h(s1)
    //                        therefore:  bal(a)=0  bal(b)=0  bal(c)=unchanged,  height(p)=-1
    //
    //
    //
    //    The second scenario is almost identical, but the heavy subtree under 'c' is
    //    now on the left child of 'b', and instead 'c' becomes the root of the subtree.
    //    Note that bal(b) could be 0, but the scenario is then identical to above.
    //    Since subtrees 's2' and 's3' are separated, we must now consider their individual
    //    heights, leading to 3 scenarios:
    //
    //
    //
    //              a  (2)                              c
    //             / \                                 / \
    //           s1   b  (-1)                        a     b
    //               / \                -->         / \   / \
    //   (-1,0,+1)  c   s4                        s1  s2 s3  s4
    //             / \
    //           s2   s3
    //
    //
    //     Cases:
    //       bal(c)=-1      h(s1)=h(s4)=h(s2)=h(s3)+1
    //                        therefore:  bal(a)=0  bal(b)=+1  bal(c)=0,   height(p)=-1
    //       bal(c)=0       h(s1)=h(s4)=h(s2)=h(s3)
    //                        therefore:  bal(a)=0  bal(b)=0  bal(c)=0,   height(p)=-1
    //       bal(c)=+1      h(s1)=h(s4)=h(s3)=h(s2)+1
    //                        therefore:  bal(a)=-1  bal(b)=0  bal(c)=0,   height(p)=-1
    //
    //  
    //    The third scenario is that the node under consideration 'a' is within reasonable
    //    balance, however the height of the subtrees may still have changed, indicating
    //    that 'p' (parent of 'a') may need a balance shift.  The 'h' height is propagated
    //    as the balance of the parent is updated.  Only when at a node the balance becomes
    //    non-zero from a subtree height change, does the height change cancel propagating
    //    (meaning, the other subtree is dominatant in determining the hight under the node)
    //

    while (top>0)
    {
        AVL_NODE *a, *b, *s1, *s2, *s3, *s4;   //  'c' and 'p' already exist.

        //  Get 'a', and 'p'.
        top-=1;
        a=stack[top];
        if (top>0)
            p=stack[top-1];
        else
            p=NULL;

        //  Which scenario are we in?
        if (AVL_getbal((*a).f)==2)
        {
            //
            //  Heavy on the 'r' side
            //
            s1=(*a).l;
            b=(*a).r;
            if (AVL_getbal((*b).f)>=0)
            {
                //
                //  Scenario 1, with the right most tree heaviest, or both
                //  subtrees 's3' and 's4' being equal.  'c' cannot be NULL
                //  otherwise bal(a) could not have been 2.
                //
                s2=(*b).l;
                c=(*b).r;
                s3=(*c).l;
                s4=(*c).r;

                //  Now stitch the trees correctly.
                //  The subtree under 'c' does not change.
                (*a).l=s1;
                (*a).r=s2;
                (*b).l=a;
                (*b).r=c;

                //  Parent 'p' might be NULL, in which case modify the root.
                if (p)
                {
                    if ((*p).l==a)
                        (*p).l=b;
                    else
                        (*p).r=b;
                }
                else
                    (*t).top=b;

                //  
                //  Set the balances:
                //
                if (AVL_getbal((*b).f)==0)
                {
                    //  bal(a)=+1  bal(b)=-1  bal(c)=unchanged, height(p)=unchanged (because s2 is +1 taller)
                    AVL_setbal((*a).f,+1);
                    AVL_setbal((*b).f,-1);
                    //  No balance update necessary for 'p', but still set 'a' to be correct.
                    h=0;
                    a=b;
                }
                else   //  Balance under 'b' was '1'
                {
                    //  bal(a)=0  bal(b)=0  bal(c)=unchanged,  height(p)=-1
                    AVL_setbal((*a).f,0);
                    AVL_setbal((*b).f,0);
                    //  Make sure that 'a' is set right, so the comparision on
                    //  the balance update is done correctly below.
                    //  (*p).l/r has now been changed from 'a' to 'c':
                    h=-1;
                    a=b;
                }
            }
            else
            {
                //
                //  This is scenario 2, with the left-most tree heaviest.
                //  Both 's2' and 's3' might be NULL, 'c' cannot be NULL.
                //
                c=(*b).l;
                s4=(*b).r;
                s2=(*c).l;
                s3=(*c).r;

                //  Sttich the rotated tree correctly, 'c' becomes root:
                (*c).l=a;
                (*c).r=b;
                (*a).r=s2;
                (*b).l=s3;

                //  Again, 'p' might be NULL.
                if (p)
                {
                    if ((*p).l==a)
                        (*p).l=c;
                    else
                        (*p).r=c;
                }
                else
                    (*t).top=c;

                //
                //  Balances:
                //
                if (AVL_getbal((*c).f)==-1)
                {
                    //  bal(a)=0  bal(b)=+1  bal(c)=0,   height(p)=-1
                    AVL_setbal((*a).f,0);
                    AVL_setbal((*b).f,+1);
                }
                else if (AVL_getbal((*c).f)==0)
                {
                    //  bal(a)=0  bal(b)=0  bal(c)=0,   height(p)=-1
                    AVL_setbal((*a).f,0);
                    AVL_setbal((*b).f,0);
                }
                else //  (AVL_getbal((*c).f)==+1)
                {
                    //  bal(a)=-1  bal(b)=0  bal(c)=0,   height(p)=-1
                    AVL_setbal((*a).f,-1);
                    AVL_setbal((*b).f,0);
                }
                //  Balance on 'c' is always 0.
                AVL_setbal((*c).f, 0);
                //  Make sure that 'a' is set right, so the comparision on
                //  the balance update is done correctly below.
                //  (*p).l/r has now been changed from 'a' to 'c':
                h=-1;
                a=c;
            }
        }
        else if (AVL_getbal((*a).f)==-2)
        {
            //
            //  Heavy on the 'l' side (symmetric to right)
            //
            s1=(*a).r;
            b=(*a).l;
            if (AVL_getbal((*b).f)<=0)
            {
                //
                //  Scenario 1, with the left most tree heaviest, or both
                //  subtrees 's3' and 's4' being equal.  'c' cannot be NULL
                //  otherwise bal(a) could not have been -2.
                //
                s2=(*b).r;
                c=(*b).l;
                s3=(*c).r;
                s4=(*c).l;

                //  Now stitch the trees correctly.
                //  The subtree under 'c' does not change.
                (*a).r=s1;
                (*a).l=s2;
                (*b).r=a;
                (*b).l=c;

                //  Parent 'p' might be NULL, in which case modify the root.
                if (p)
                {
                    if ((*p).l==a)
                        (*p).l=b;
                    else
                        (*p).r=b;
                }
                else
                    (*t).top=b;

                //  
                //  Set the balances:
                //
                if (AVL_getbal((*b).f)==0)
                {
                    //  bal(a)=-1  bal(b)=+1  bal(c)=unchanged, height(p)=unchanged (because s2 is +1 taller)
                    AVL_setbal((*a).f,-1);
                    AVL_setbal((*b).f,+1);
                    //  No balance update necessary for 'p', but still set 'a' to be correct.
                    h=0;
                    a=b;
                }
                else   //  Balance under 'b' was '-1'
                {
                    //  bal(a)=0  bal(b)=0  bal(c)=unchanged,  height(p)=-1
                    AVL_setbal((*a).f,0);
                    AVL_setbal((*b).f,0);
                    //  Make sure that 'a' is set right, so the comparision on
                    //  the balance update is done correctly below.
                    //  (*p).l/r has now been changed from 'a' to 'c':
                    h=-1;
                    a=b;
                }
            }
            else
            {
                //
                //  This is scenario 2, with the left-most tree heaviest.
                //  Both 's2' and 's3' might be NULL, 'c' cannot be NULL.
                //
                c=(*b).r;
                s4=(*b).l;
                s2=(*c).r;
                s3=(*c).l;

                //  Sttich the rotated tree correctly, 'c' becomes root:
                (*c).r=a;
                (*c).l=b;
                (*a).l=s2;
                (*b).r=s3;

                //  Again, 'p' might be NULL.
                if (p)
                {
                    if ((*p).l==a)
                        (*p).l=c;
                    else
                        (*p).r=c;
                }
                else
                    (*t).top=c;

                //
                //  Balances:
                //
                if (AVL_getbal((*c).f)==+1)
                {
                    //  bal(a)=0  bal(b)=+1  bal(c)=0,   height(p)=-1
                    AVL_setbal((*a).f,0);
                    AVL_setbal((*b).f,-1);
                }
                else if (AVL_getbal((*c).f)==0)
                {
                    //  bal(a)=0  bal(b)=0  bal(c)=0,   height(p)=-1
                    AVL_setbal((*a).f,0);
                    AVL_setbal((*b).f,0);
                }
                else //  (AVL_getbal((*c).f)==+1)
                {
                    //  bal(a)=-1  bal(b)=0  bal(c)=0,   height(p)=-1
                    AVL_setbal((*a).f,+1);
                    AVL_setbal((*b).f,0);
                }
                //  Balance on 'c' is always 0.
                AVL_setbal((*c).f, 0);
                //  Make sure that 'a' is set right, so the comparision on
                //  the balance update is done correctly below.
                //  (*p).l/r has now been changed from 'a' to 'c':
                h=-1;
                a=c;
            }
        }
        //  Else, no rotations necessary.


        //  Is there still a height change?
        if (h!=0)
        {
            //  We are propagating a height change on the subtree.
            //  Check the balances of the parent node 'p' and
            //  adjust accordingly.
            if (p)
            {
                //  Note that 'a' was reset correctly if a rotation was performed.
                if ((*p).l==a)
                    AVL_incbal((*p).f);
                else
                    AVL_decbal((*p).f);

                //  At this point, if bal(p) is now zero, it means that height
                //  under 'p' was lost, otherwise, height under 'p' is dominated
                //  by the other (non-rotated) subtree.
                if (AVL_getbal((*p).f)==0)
                    h=-1;
                else
                    h=0;
            }
            //  No 'p' means we are at the top of the tree, which is
            //  handled below, and outside of this while-block.
        }
    }


    //  If we are still propagating a height after
    //  reaching the root of the tree, record it now:
    //  This specificially needs to be outside of the balance
    //  section for 1 and 2-node trees.
    if (h<0)
        (*t).height-=1;

    //  And the data pointer, if found:
    return(d);
}








/************************************************************************
 *                                                                      *
 *   Testing and validation                                             *
 *                                                                      *
 ************************************************************************/



//
//  Method that walks the tree left-root-right (w/o recursion).
//  Callback is called for each node in the sorted order of the nodes.
//
void AVL_walk(AVL_TREE *t, void (*callback)(void *d, void *user), void *user)
{
    AVL_NODE *c, *p;
    AVL_NODE *stack[AVL_MAX_DEPTH];
    int top;

    //  Only do this if there's a tree:
    if ((*t).top==NULL)
        return;
        
    //  Note that '(*t).top' ends up on the stack twice, but it avoids more if-then-else
    top=1;
    stack[0]=(*t).top;
    c=(*t).top;
    p=c;

    //  While top!=0
    do
    {
        //  We just recursed down into this node, if 'prev'
        //  is our parent, which we know from the stack.
        if (stack[top-1]==p)
        {
            //  Do the operation here -- for root-left-right:
            //  NOTE:  this is NOT the sorted order.
            //callback((*c).d, user);
            if ((*c).l)
            {
                //  Recurse left
                stack[top]=c;
                top+=1;
                p=c;
                c=(*c).l;
            }
            else
                p=NULL;     //  Triggers the next immediate section
        }
        //  We just popped up, if 'prev' is one of our
        //  children.  If it is the left, then now we
        //  move to the right.
        if (p==(*c).l)
        {
            //
            //  Operation here -- for left-root-right 
            //  Which is the sorted sequence.
            //
            callback((*c).d, user);

            // Recurse right
            if ((*c).r)
            {
                stack[top]=c;
                top+=1;
                p=c;
                c=(*c).r;
            }
            else
                p=NULL;     //  Triggers the next immediate section
        }
        //  Previous node is right, we must now move up the stack.
        if (p==(*c).r)
        {
            top-=1;
            p=c;
            c=stack[top];
            //  Else we're done.
        }
    }
    while (top>0 && top<AVL_MAX_DEPTH-1);       //  -1 is overflow protection


    return;
}




//
//  Print the tree to stdout SVG with HTML header.
//
//  How this could be made better:
//   - 'rect' bounding box around the text
//   - 2-pass:  draw the tree lines first, then the text boxes
//   - reduce 'dy' as we get deeper in the tree
//
void AVL_print(AVL_TREE *t, int x, int y, void (*printLabel)(FILE *stream, void *d))
{
    AVL_NODE *c, *p;
    AVL_NODE *stack[AVL_MAX_DEPTH];
    int top;
    int cx, cy, dx, dy;   //  Current x,y, delta x,y

    //  Note that '(*t).top' ends up on the stack twice, but it avoids more if-then-else
    top=1;
    stack[0]=(*t).top;
    c=(*t).top;
    p=c;

    //  Start in the middle, at the top:
    //  'dx' is recalculated at each step
    //  Consider reducing 'x' to '0.9*x' before starting to avoid running into the sides of the canvas.
    cx=x/2;
    dy=y/((*t).height+1);   //  The 'height' is increased by 1 to keep some margin at the top and the bottom
    cy=dy/2;

    //  HTML/SVG header:
    fprintf(stdout, "<!DOCTYPE html>\n<html>\n<body>\n");
    fprintf(stdout, "<svg height=\"%i\" width=\"%i\">\n", y, x);
    if ((*t).top==NULL)
    {
        fprintf(stdout, "</svg>\n</html>\n</body>\n");
        return;
    }

    //  While top!=0
    do
    {
        //  We just recursed down into this node, if 'prev'
        //  is our parent, which we know from the stack.
        if (stack[top-1]==p)
        {
            //
            //  Print the location and the label.
            //
            fprintf(stdout, "<text x=\"%i\" y=\"%i\" fill=black>", cx, (top-1)*dy+dy/2);
            printLabel(stdout, (*c).d);
            fprintf(stdout, "</text>\n");
            //  Recurse left:
            if ((*c).l)
            {
                //  Smaller dx steps.
                dx=x>>(top+1);
                //  Print link to left child:
                fprintf(stdout, "<line x1=\"%i\" y1=\"%i\" x2=\"%i\" y2=\"%i\" style=\"stroke:rgb(0,0,128);stroke-width:1\" />\n", cx, (top-1)*dy+dy/2, cx-dx, (top)*dy+dy/2);
                cx-=dx;
                
                //  Recurse left
                stack[top]=c;
                top+=1;
                p=c;
                c=(*c).l;
            }
            else
                p=NULL;     //  Triggers the next immediate section
        }
        //  We just popped up, if 'prev' is one of our
        //  children.  If it is the left, then now we
        //  move to the right.
        if (p==(*c).l)
        {
            //  After all, if we were on the left, then now we've come
            //  up and should adjust our cx by the right amount:
            if (p!=NULL)
            {
                dx=x>>(top+1);
                cx+=dx;
            }

            if ((*c).r)
            {
                //  Smaller dx steps.
                dx=x>>(top+1);
                //  Print link to left child:
                fprintf(stdout, "<line x1=\"%i\" y1=\"%i\" x2=\"%i\" y2=\"%i\" style=\"stroke:rgb(0,0,128);stroke-width:1\" />\n", cx, (top-1)*dy+dy/2, cx+dx, (top)*dy+dy/2);
                cx+=dx;

                //  Recurse right:
                stack[top]=c;
                top+=1;
                p=c;
                c=(*c).r;
            }
            else
                p=NULL;     //  Triggers the next immediate section
        }
        //  Previous node is right, we must now move up the stack.
        if (p==(*c).r)
        {
            //  How do we "back out" of our cx changes?
            if (p!=NULL)
            {
                dx=x>>(top+1);
                cx-=dx;
            }

            //  Move up:
            top-=1;
            p=c;
            c=stack[top];
        }
    }
    while (top>0 && top<AVL_MAX_DEPTH-1);       //  -1 is overflow protection
    //  The end:
    fprintf(stdout, "</svg>\n</html>\n</body>\n");
    return;
}








//////////////////////////////////////////////////////////////////////////////////
//
//  A check-balance method simply returns an error.
//
//
//

//
//  Recursive method to validate balances and heights
//  Returns height of subtree it was given to.
//  Prints error message on failure.
//
int AVL_checkBalance(AVL_NODE *n)
{
    int l,r, a, b;
    if (n==NULL) return(0);
    l=AVL_checkBalance((*n).l);
    r=AVL_checkBalance((*n).r);
    if (l==-1 || r==-1)
        return(-1);
    b=r-l;
    a=(int)AVL_getbal((*n).f);
    if (b!=a || b<-1 || b>+1)
    {
        fprintf(stderr, "BALANCE ERROR on %i:  l=%i r=%i (b=%i) f=%i\n", *((int*)(*n).d), l, r, b, a);
        return(-1);
    }
    if (b>0)
        return(r+1);
    return(l+1);
}












