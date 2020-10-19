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
 *  Example main, and load test.
 *
 */

#include "avl.h"
#include <time.h>
#include <pthread.h>


//  The actual thread count is determined in main...
#define AVL_EXAMPLE_MAX_THREAD 512


//
//  Takes an array of 'n' integers, tries n random
//  inserts, then runs the array and tries all of them
//  in sequence to grab the stragglers.
//
int AVL_testFill(AVL_TREE *t, int *a, int n)
{
    int i=0;
    int s=0;    //  Sucessful inserts
    int f=0;    //  Failed inserts
    int rc;
    //  Initialize with negative numbers
    for (i=0; i<n; i+=1)
        a[i]=-i-1;
    //  Initial fill:
    for (i=0; i<n; i+=1)
    {
        //  Pick a random number:
        unsigned *seed=(unsigned*)(*t).user;
        int r=rand_r(seed)%n;

        //  Non-inserted numbers are made negative.
        //  Inserted ones are positive:
        if (a[r]<0)
        {
            a[r]=-a[r];
            rc=AVL_insert(t, &(a[r]));
            if (rc!=0)
                fprintf(stderr, "ERROR (%i): %i failed to insert, but was not yet in the tree!\n", rc, a[r]);
            else
                s+=1;
        }
        else
        {
            //  Already in the tree
            rc=AVL_insert(t, &(a[r]));
            if (rc!=1)
                fprintf(stderr, "ERROR (%i): %i was inserted, but was already in the tree!\n", rc, a[r]);
            else
                f+=1;
        }
    }
    s=0;
    f=0;
    for (i=0; i<n; i+=1)
    {
        //  Stragglers are inserted.
        if (a[i]<0)
        {
            a[i]=-a[i];
            rc=AVL_insert(t, &(a[i]));
            if (rc!=0)
                fprintf(stderr, "ERROR (%i): %i failed to insert, but was not yet in the tree!\n", rc, a[i]);
            else
                s+=1;
        }
        else
        {
            //  Already in the tree
            rc=AVL_insert(t, &(a[i]));
            if (rc!=1)
                fprintf(stderr, "ERROR (%i): %i was inserted, but was already in the tree!\n", rc, a[i]);
            else
                f+=1;
        }
    }
    return(0);
}

//
//  Takes the same array of 'n' integers, tries n random
//  deletes, then runs the array and tries all of them
//  in sequence to grab the stragglers.  This is a heavier
//  method, as it performs a balance check on each succesful
//  delete from the tree
//
int AVL_testDrain(AVL_TREE *t, int *a, int n)
{
    int i=0;
    int s=0;    //  Sucessful inserts
    int f=0;    //  Failed inserts
    int rc;
    int h;
    //  Drain some numbers:
    for (i=0; i<n; i+=1)
    {
        //  Pick a random number:
        unsigned *seed=(unsigned*)(*t).user;
        int r=rand_r(seed)%n;

        //  Succesfully deleted numbers are made negative.
        if (a[r]>0)
        {
            if (AVL_delete(t, &(a[r]))==NULL)
            {
                fprintf(stderr, "ERROR: %i failed to delete, but was in the tree!\n", a[r]);
                exit(1);
            }
            else
                s+=1;
            a[r]=-a[r];     //  Mark it negative
            //  Check balance:
            h=AVL_checkBalance((*t).top);
            if (h!=(*t).height) 
            {
                fprintf(stderr, "Height/balance error:  (*t).height=%i  checkbal=%i\n", (*t).height, h);
                exit(1);
            }
        }
        else
        {
            //  Already deleted, see if another delete attempt fails:
            a[r]=-a[r];
            if (AVL_delete(t, &(a[r]))!=NULL)
            {
                fprintf(stderr, "ERROR: %i was succesfully deleted, but was no longer in the tree!\n", a[r]);
                exit(1);
            }
            else
                f+=1;
            a[r]=-a[r]; // Reset
        }

        //  Every so often, see if blocks can be freed:
        if (i%(*t).allocAtOnce==0)
            AVL_dealloc(t);
    }
    s=0;
    f=0;
    for (i=0; i<n; i+=1)
    {
        //  Stragglers are deleted.
        if (a[i]>0)
        {
            if (AVL_delete(t, &(a[i]))==NULL)
                fprintf(stderr, "ERROR: %i failed to delete, but was in the tree!\n", a[i]);
            else
                s+=1;
            a[i]=-a[i];     //  Mark it negative
            //  Check balance:
            h=AVL_checkBalance((*t).top);
            if (h!=(*t).height) 
            {
                fprintf(stderr, "Height/balance error:  (*t).height=%i  checkbal=%i\n", (*t).height, h);
                exit(1);
            }
        }
        else
        {
            //  Already deleted, see if another delete attempt fails:
            a[i]=-a[i];
            if (AVL_delete(t, &(a[i]))!=NULL)
            {
                fprintf(stderr, "ERROR: %i was succesfully deleted, but was no longer in the tree!\n", a[i]);
                exit(1);
            }
            else
                f+=1;
            a[i]=-a[i]; // Reset
        }

        //  Every so often, see if blocks can be freed:
        if (i%(*t).allocAtOnce==0)
            AVL_dealloc(t);
    }
    return(0);
}






//
//  Template methods for any object where the first var
//  is an integer which is also used as the key.
//
void printLabel(FILE *stream, void *d)
{
    fprintf(stream, "%i", *((int*)d));
}

void callback(void *d, void *user)
{
    int i=*((int*)d);
    int j=*((int*)user);
    if (i<=j)
    {
        fprintf(stderr, "ERROR:  non-sequential sorting order detected!  (i<=j: %i<=%i)\n", i, j);
        exit(1);
    }
    *((int*)user)=i;
    //fprintf(stderr, "%i ", i);
}


//
//  Compare 2 integers:
//
int exampleEval(void *data1, void *data2, void *user)
{
    return((*((int*)data2))-(*((int*)data1)));
}



//
//  Global state to determine 'rank'
//
typedef struct
{
    pthread_t tid[AVL_EXAMPLE_MAX_THREAD];
    int rank;       //  To determine some order in the threads
    pthread_mutex_t rankLock;
    int nt;         //  Num threads
}
AVL_EXAMPLE_STRUCT;



//
//  Testing thread.  The threads rank decides which 
//  specific sequence to run, initialized with 'rand_r'
//
#define AVL_TEST_NUM 17000
#define AVL_TEST_SIZ 170
void *workerThread(void *user)
{
    int i,j;
    int *a;
    long ts;
    int h;
    long seed=0;    //  Seed of rand_r
    int rank;
    AVL_EXAMPLE_STRUCT *e=(AVL_EXAMPLE_STRUCT*) user;


        //  Determine the 'rank' of this thread.
    pthread_mutex_lock(&((*e).rankLock));
    {
        rank=(*e).rank;
        (*e).rank+=1;
    }
    pthread_mutex_unlock(&((*e).rankLock));
    //fprintf(stderr, "Thread ID %i\n", rank);

        //  Create the tree, the 'user' pointer is the
        //  seed for the rand_r method.
    AVL_TREE *t=AVL_newTree(32, exampleEval, &seed);
    a=(int*)malloc(AVL_TEST_NUM*sizeof(int));

    //  Each thread starts with a different place in the random
    //  sequence to get maximum coverage of different tests:
    for (j=rank; j<AVL_TEST_NUM; j+=(*e).nt)
    {
        seed=j;
        for (i=1; i<AVL_TEST_SIZ; i+=1)
        {
            int k=0;
            AVL_testFill(t, a, i);
            //AVL_print(t, 1300, 400, printLabel);
            AVL_walk(t, callback, &k);
            AVL_testDrain(t, a, i);
        }
    }

    AVL_destroy(t);
    free(a);
    return(NULL);
}

//
//  Sample main and unit test:
//
int main(int argc, char **argv)
{
    AVL_EXAMPLE_STRUCT e;
    void *retval;   //  Will be NULL.
    int i;

    memset(&e, 0, sizeof(AVL_EXAMPLE_STRUCT));
    pthread_mutex_init(&e.rankLock, NULL);
    e.nt=16;

    //  Create the workers
    for (i=0; i<e.nt; i+=1)
        pthread_create(&e.tid[i], NULL, workerThread, &e);

    //  Join them back in
    for (i=0; i<e.nt; i+=1)
        pthread_join(e.tid[i], &retval);


    pthread_mutex_destroy(&e.rankLock);
    return(0);
}








