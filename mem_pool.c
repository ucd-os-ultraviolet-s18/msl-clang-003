/*
 * Created by Ivo Georgiev on 2/9/16.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h> // for perror()

#include "mem_pool.h"

/*************/
/*           */
/* Constants */
/*           */
/*************/
static const float      MEM_FILL_FACTOR                 = 0.75;
static const unsigned   MEM_EXPAND_FACTOR               = 2;

static const unsigned   MEM_POOL_STORE_INIT_CAPACITY    = 20;
static const float      MEM_POOL_STORE_FILL_FACTOR      = 0.75;
static const unsigned   MEM_POOL_STORE_EXPAND_FACTOR    = 2;

static const unsigned   MEM_NODE_HEAP_INIT_CAPACITY     = 40;
static const float      MEM_NODE_HEAP_FILL_FACTOR       = 0.75;
static const unsigned   MEM_NODE_HEAP_EXPAND_FACTOR     = 2;

static const unsigned   MEM_GAP_IX_INIT_CAPACITY        = 40;
static const float      MEM_GAP_IX_FILL_FACTOR          = 0.75;
static const unsigned   MEM_GAP_IX_EXPAND_FACTOR        = 2;



/*********************/
/*                   */
/* Type declarations */
/*                   */
/*********************/
typedef struct _alloc {
    char *mem;
    size_t size;
} alloc_t, *alloc_pt;

typedef struct _node {
    alloc_t alloc_record;
    unsigned used;
    unsigned allocated;
    struct _node *next, *prev; // doubly-linked list for gap deletion
} node_t, *node_pt;

typedef struct _gap {
    size_t size;
    node_pt node;
} gap_t, *gap_pt;

typedef struct _pool_mgr {
    pool_t pool;
    node_pt node_heap;
    unsigned total_nodes;
    unsigned used_nodes;
    gap_pt gap_ix;
    unsigned gap_ix_capacity;
} pool_mgr_t, *pool_mgr_pt;



/***************************/
/*                         */
/* Static global variables */
/*                         */
/***************************/
static pool_mgr_pt *pool_store = NULL; // an array of pointers, only expand
static unsigned pool_store_size = 0;
static unsigned pool_store_capacity = 0;



/********************************************/
/*                                          */
/* Forward declarations of static functions */
/*                                          */
/********************************************/
static alloc_status _mem_resize_pool_store();
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr);
static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status
        _mem_add_to_gap_ix(pool_mgr_pt pool_mgr,
                           size_t size,
                           node_pt node);
static alloc_status
        _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                size_t size,
                                node_pt node);
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status _mem_invalidate_gap_ix(pool_mgr_pt pool_mgr);



/****************************************/
/*                                      */
/* Definitions of user-facing functions */
/*                                      */
/****************************************/
/////////////////////////////////////////////////////////////////////////////////////////////////////
alloc_status mem_init() {
    // ensure that it's called only once until mem_free
    // allocate the pool store with initial capacity
    // note: holds pointers only, other functions to allocate/deallocate
int i;
    if (pool_store == NULL) {

            pool_store = malloc(MEM_POOL_STORE_INIT_CAPACITY * sizeof(pool_mgr_pt));
            pool_store_size = 1;
            pool_store_capacity = MEM_POOL_STORE_INIT_CAPACITY;
        for (i = 0; i < pool_store_capacity; i++)
        {
            pool_store[i] = NULL;
        }
            return ALLOC_OK;
    }

    else
    {
        return ALLOC_CALLED_AGAIN;
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
alloc_status mem_free() {
    int i;


    if (pool_store == NULL){
        return ALLOC_CALLED_AGAIN;
    }
    // ensure that it's called only once for each mem_init
    // make sure all pool managers have been deallocated

    // can free the pool store array
    // update static variables
    if (pool_store != NULL && pool_store_size != 0){

        for(i = 0; i < pool_store_size; i++)
        {
            if (pool_store[i]&& pool_store[i] != NULL)
            {
                free(pool_store[i]);
                pool_store[i] = NULL;
            }
        }



        free(pool_store);
        pool_store_capacity = 0;
        pool_store_size = 0;

        for (i = 0; i < pool_store_capacity; i++)
        {
            pool_store[i] = NULL;
        }

        pool_store = NULL;
        return ALLOC_OK;

    }
    else
    {
        return ALLOC_CALLED_AGAIN;
    }

    return ALLOC_FAIL;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
pool_pt mem_pool_open(size_t size, alloc_policy policy) {
    alloc_status status;

    // make sure there the pool store is allocated

    if (pool_store == NULL)
    {
        return NULL;
    }

    // expand the pool store, if necessary

    _mem_resize_pool_store();


    // allocate a new mem pool mgr
    // check success, on error return null
    int i;

    pool_mgr_pt newPool = malloc(sizeof(pool_mgr_t));


    // allocate a new node heap
    // check success, on error deallocate mgr/pool and return null

    newPool->node_heap = malloc(MEM_NODE_HEAP_INIT_CAPACITY*sizeof(node_t));

    if (newPool->node_heap == NULL)
    {
        free(pool_store[pool_store_size]);
        return NULL;
    }


    for (i = 0; i < MEM_NODE_HEAP_INIT_CAPACITY; i++)
    {
        newPool->node_heap[i].next = NULL;
        newPool->node_heap[i].prev = NULL;
        newPool->node_heap[i].allocated = 0;
        newPool->node_heap[i].used = 0;
        newPool->node_heap[i].alloc_record.size = 0;
        newPool->node_heap[i].alloc_record.mem = NULL;
    }
    newPool->total_nodes = MEM_NODE_HEAP_INIT_CAPACITY;


    // allocate a new gap index
    // check success, on error deallocate mgr/pool/heap and return null

    newPool->gap_ix= malloc(MEM_GAP_IX_INIT_CAPACITY*sizeof(gap_t));

    if (newPool->gap_ix == NULL)
    {
        if (newPool->node_heap)
        {
            free(newPool->node_heap);
        }
        if(newPool->gap_ix)
        {
            free(newPool->gap_ix);
        }
        free(pool_store[pool_store_size]);
        return NULL;
    }

    for (i = 0; i < MEM_GAP_IX_INIT_CAPACITY; i++)
    {
        newPool->gap_ix[i].node = (node_pt)malloc(sizeof(node_t));
        newPool->gap_ix[i].node->prev = NULL;
        newPool->gap_ix[i].node->next = NULL;
        newPool->gap_ix[i].node->alloc_record.size = 0;
        newPool->gap_ix[i].node->alloc_record.mem= NULL;
        newPool->gap_ix[i].node->used = 0;
        newPool->gap_ix[i].node->allocated= 0;
        newPool->gap_ix[i].size = 0;
    }
    newPool->gap_ix_capacity = MEM_GAP_IX_INIT_CAPACITY;



    // assign all the pointers and update meta data:
    //   initialize top node of node heap

    newPool->used_nodes= 1;

    newPool->node_heap->alloc_record.mem = malloc(size);

    if (newPool->node_heap->alloc_record.mem == NULL)
    {
        newPool->node_heap->alloc_record.mem = realloc(newPool->node_heap->alloc_record.mem,size);

    }

    newPool->node_heap->alloc_record.size = size;
    newPool->node_heap->allocated = 0;
    newPool->node_heap->used = 1;


    //   initialize top node of gap index
    newPool->gap_ix[0].node = newPool->node_heap;
    newPool->gap_ix[0].size = size;

    //   initialize pool mgr

    newPool->pool.mem = newPool->node_heap->alloc_record.mem;// address via &??
    newPool->pool.policy = policy;
    newPool->pool.total_size = size;
    newPool->pool.alloc_size = 0;//????
    newPool->pool.num_allocs = 0;
    newPool->pool.num_gaps = 1;// was 1???

    //   link pool mgr to pool store
    pool_store[pool_store_size] = newPool;

    pool_store_size++;
    // return the address of the mgr, cast to (pool_pt)

    return ((pool_pt)(newPool));//((pool_pt)(pool_store[pool_store_size]));

}
/////////////////////////////////////////////////////////////////////////////////////////////////////
alloc_status mem_pool_close(pool_pt pool) {
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt manager = ((pool_mgr_pt)pool);

    // check if this pool is allocated

    if (manager->pool.mem == NULL)
    {
        return ALLOC_FAIL;
    }

    // check if this pool only has one gap
    if (manager->pool.num_gaps != 1)
    {
        return ALLOC_FAIL;
    }
    if (manager->pool.num_allocs != 0)
    {
        return ALLOC_NOT_FREED;
    }
    // check if it has zero allocations

    // free memory pool
    // free node heap
    // free gap index
    // find mgr in pool store and set to null
    // note: don't decrement pool_store_size, because it only grows
    // free mgr

    // frees all of the attributes of the manager
    int i ;
    free(manager->pool.mem);
    manager->pool.mem = NULL;
    free(manager->node_heap);
    free(manager->gap_ix);
    manager->node_heap =NULL;
    manager->gap_ix = NULL;



    for (i = 0; i < pool_store_size; i++)
    {
        if (pool_store[i] ==manager)
        {

            pool_store[i] = NULL;

            break;
        }
    }

    free(manager);
    return ALLOC_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
// This function performs a single allocation of size in bytes from the given memory pool
// Allocations from different memory pools are independent
// Note: There is no mechanism for bounds-checking on the use of the allocations

void * mem_new_alloc(pool_pt pool, size_t size) {

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt managerPtr = ((pool_mgr_pt)pool);

    // check if any gaps, return null if none
    if (pool->num_gaps == 0) return NULL;

    // if BEST_FIT:
    // traverse through gap_ix array and look for first spot that the node
    // will fit in
    if (pool->policy == BEST_FIT) {

        // this checks the gap index for the first size
        for (int i = 0; i < pool->num_gaps; i++) {
            if(managerPtr->gap_ix[i].size >= size) { // if size of gap element is large enough

                size_t gapSize = managerPtr->gap_ix[i].size;

                // sets the new node to the current gap pointer
                node_pt newNode = managerPtr->gap_ix[i].node;

                // remove the node from the gap index
                _mem_remove_from_gap_ix(managerPtr, newNode->alloc_record.size, newNode);

                // if there is space left over, then we need to use another node to
                // represent the gap
                if (gapSize > size) {

                    // look for next available node to hold the gap
                    int j = 0;
                    while (managerPtr->node_heap[j].used == 1) j++;

                    node_pt newGapPtr = &(managerPtr->node_heap[j]);

                    // sets new gap's attributes
                    newGapPtr->used = 1;
                    newGapPtr->alloc_record.size = gapSize;
                    newGapPtr->next = newNode->next;

                    // set the gap to go after the allocated node
                    // newnode prev should still be fine

                    newNode->next = newGapPtr;

                    // set gap node after new node
                    newNode->prev = newNode;

                    // add the new (smaller) gap to the gap index
                    _mem_add_to_gap_ix(managerPtr, gapSize, newGapPtr);

                }

                // do not need to reset newNode pointers if it takes up the entire allocation

                // set newNode attributes:
                newNode->alloc_record.size = size;
                newNode->used = 1;
                newNode->allocated = 1;

                // update pool info
                pool->num_allocs++;
                pool->alloc_size = pool->alloc_size + size;

                // return the user-requested memory
                return newNode->alloc_record.mem;

            }
            else {
                printf("No room for node!");
                return NULL;
            } // else there's no room for it
        }

        return NULL; // returns null if a space wasn't found
    }


    // with first fit, we traverse the linked list and find the first gap node
    // where we can put stuff (size doesn't matter)
    else if (pool->policy == FIRST_FIT) {

        node_pt nodePtr = managerPtr->node_heap; // set ptr to point at first element in node heap

        // traverse until we find a node that isn't used and isn't too small
        while (nodePtr->allocated == 1 || nodePtr->alloc_record.size < size) {


            nodePtr = nodePtr->next;
        }

        if (nodePtr == NULL) {
            printf("No room for node!");
            return NULL;
        }

        // set new gapsize if there is one
        size_t gapSize = nodePtr->alloc_record.size - size;

        // after this line, k will be index of gap pointer to get rid of
        int k = 0;
        while (nodePtr != managerPtr->gap_ix[k].node && k < pool->num_gaps) k++;

        // see if where we stopped is correct
        if (nodePtr != managerPtr->gap_ix[k].node) {
            printf("Could not find node in gap index");
            return NULL;
        }

        // remove from the gap index
        _mem_remove_from_gap_ix(managerPtr, nodePtr->alloc_record.size, managerPtr->gap_ix[k].node);

        // adjust pool attributes
        pool->num_gaps--;

        // if gap size is bigger than zero, add a gap node
        if (gapSize > 0) {

            // look for next available node to hold the gap
            int j = 0;
            while (managerPtr->node_heap[j].used == 1) j++;

            node_pt newGapPtr = &(managerPtr->node_heap[j]);

            // sets new gap's attributes
            newGapPtr->used = 1;
            newGapPtr->alloc_record.size = gapSize;
            newGapPtr->next = nodePtr->next;

            // set the gap to go after the allocated node
            // newnode prev should still be fine

            nodePtr->next = newGapPtr;

            // set gap node after new node
            newGapPtr->prev = nodePtr;

            // add the new (smaller) gap to the gap index
            _mem_add_to_gap_ix(managerPtr, gapSize, newGapPtr);
        }

        // do not need to reset newNode pointers if it takes up the entire allocation

        // set newNode attributes:
        nodePtr->alloc_record.size = size;
        nodePtr->used = 1;
        nodePtr->allocated = 1;

        // set pool attributes
        pool->num_allocs++;
        pool->alloc_size = pool->alloc_size + size;

        // return the user-requested memory
        return nodePtr->alloc_record.mem;
    }



        // check used nodes fewer than total nodes, quit on error
        // get a node for allocation:
        // if FIRST_FIT, then find the first sufficient node in the node heap
        // if BEST_FIT, then find the first sufficient node in the gap index
        // check if node found
        // update metadata (num_allocs, alloc_size)
        // calculate the size of the remaining gap, if any
        // remove node from gap index
        // convert gap_node to an allocation node of given size
        // adjust node heap:
        //   if remaining gap, need a new node
        //   find an unused one in the node heap
        //   make sure one was found
        //   initialize it to a gap node
        //   update metadata (used_nodes)
        //   update linked list (new node right after the node for allocation)
        //   add to gap index
        //   check if successful
        // return allocation record by casting the node to (alloc_pt)

        return NULL;
    }

    // This function deallocates the given allocation from the given memory pool
    alloc_status mem_del_alloc(pool_pt pool, void * alloc) {

        // get mgr from pool by casting the pointer to (pool_mgr_pt)
        pool_mgr_pt managerPtr = ((pool_mgr_pt)pool);

        // get node from alloc by casting the pointer to (node_pt)
        node_pt deletePtr = ((node_pt)alloc);

        // find the node in the node heap
        // this is node-to-delete
        node_pt nodePtr = managerPtr->node_heap; // point at head of linked list
        while (nodePtr != deletePtr) nodePtr = nodePtr->next; // traverse to find node

        // if we've gone to the end of the list and not found it
        if (nodePtr == NULL) {
            printf("Node to delete not found in memory pool\n");
            return ALLOC_NOT_FREED;
        }

        // convert to gap node
        nodePtr->allocated = 0;
        nodePtr->alloc_record.mem = NULL;

        // add gap to gap index
        _mem_add_to_gap_ix(managerPtr, nodePtr->alloc_record.size, nodePtr);

        // update metadata (num_allocs, alloc_size)
        pool->num_allocs--;
        pool->alloc_size = pool->alloc_size - nodePtr->alloc_record.size;



        // if the next node in the list is also a gap, merge into node-to-delete

        if (nodePtr->next->allocated == 0) {

            node_pt extraGap = nodePtr->next;

            //   add the size to the node-to-delete
            nodePtr->alloc_record.size =
                    nodePtr->alloc_record.size + extraGap->alloc_record.size;

            //   remove the next node from gap index
            _mem_remove_from_gap_ix(managerPtr, extraGap->alloc_record.size, extraGap);

            //   check success
            // traverse the array to see if we can find the node or reach the end of the array
            int i = 0;
            while (managerPtr->gap_ix[i].node != extraGap && i < managerPtr->gap_ix_capacity) i++;
            if (managerPtr->gap_ix[i].node == extraGap) {
                printf("Error: node not deleted from gap index");
                return ALLOC_NOT_FREED; // ideally this should never happen
            }

            // update old gapnode as unused
            extraGap->alloc_record.size = 0;
            extraGap->alloc_record.mem = NULL; // should already be null but eh
            extraGap->used = 0;
            extraGap->prev = NULL;

            // connects the new gap to the node after the merged gap
            nodePtr->next = extraGap->next;

            extraGap->next = NULL;

            // update metadata of pool
            pool->num_gaps--;
        }

        // -----------------------------------------------------------------------------------
        // if the previous node is a gap node
        if (nodePtr->prev->allocated == 0) {

            node_pt extraGap = nodePtr->prev;

            //   add the size to the node-to-delete
            nodePtr->alloc_record.size =
                    nodePtr->alloc_record.size + extraGap->alloc_record.size;

            //   remove the next node from gap index
            _mem_remove_from_gap_ix(managerPtr, extraGap->alloc_record.size, extraGap);

            //   check success
            // traverse the array to see if we can find the node or reach the end of the array
            int i = 0;
            while (managerPtr->gap_ix[i].node != extraGap && i < managerPtr->gap_ix_capacity) i++;
            if (managerPtr->gap_ix[i].node == extraGap) {
                printf("Error: node not deleted from gap index");
                return ALLOC_NOT_FREED; // ideally this should never happen
            }

            // update old gapnode as unused
            extraGap->alloc_record.size = 0;
            extraGap->alloc_record.mem = NULL; // should already be null but eh
            extraGap->used = 0;

            // set the previous on nodePtr
            // then remove previous on gap
            nodePtr->prev = extraGap->prev;
            extraGap->prev = NULL;

            // nodeptr->next is already pointing to correct thing
            extraGap->next = NULL;

            // keep track of correct number of gaps
            pool->num_gaps--;

        }

        // this merged node-to-delete might need to be added to the gap index
        // but one more thing to check...
        // if the previous node in the list is also a gap, merge into previous!
        //   remove the previous node from gap index
        //   check success
        //   add the size of node-to-delete to the previous
        //   update node-to-delete as unused
        //   update metadata (used_nodes)
        //   change the node to add to the previous node!
        // add the resulting node to the gap index

        // organize gap index
        _mem_sort_gap_ix(managerPtr);

        // check success
        nodePtr = managerPtr->node_heap; // point nodePtr back at the head

        // look for the new gapnode in the linked list
        while (nodePtr != deletePtr) nodePtr = nodePtr->next;
        if (nodePtr == NULL) {
            printf("Cannot find gap node after deallocation");
            return ALLOC_NOT_FREED;
        }



        return ALLOC_OK;
    }


void mem_inspect_pool(pool_pt pool,
                      pool_segment_pt *segments,
                      unsigned *num_segments) {
    // get the mgr from the pool
    // allocate the segments array with size == used_nodes
    // check successful
    // loop through the node heap and the segments array
    //    for each node, write the size and allocated in the segment
    // "return" the values:
    /*
                    *segments = segs;
                    *num_segments = pool_mgr->used_nodes;
     */
}



/***********************************/
/*                                 */
/* Definitions of static functions */
/*                                 */
/***********************************/
static alloc_status _mem_resize_pool_store() {
    // check if necessary
    /*
                if (((float) pool_store_size / pool_store_capacity)
                    > MEM_POOL_STORE_FILL_FACTOR) {...}
     */
    // don't forget to update capacity variables


    if (((float) pool_store_size / pool_store_capacity) > MEM_POOL_STORE_FILL_FACTOR) {

        pool_mgr_pt * temp = realloc(pool_store, pool_store_capacity * MEM_POOL_STORE_EXPAND_FACTOR
                                         * sizeof(pool_mgr_pt));

        if (!temp)
        {
            return ALLOC_FAIL;
        }

        else
        {
            pool_store = temp;
        }
        pool_store_capacity = MEM_POOL_STORE_EXPAND_FACTOR * MEM_POOL_STORE_INIT_CAPACITY;

        return ALLOC_OK;
    }
    else
    {
        return ALLOC_FAIL;
    }

}


static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr) {

    int i;

    int item[pool_mgr->gap_ix_capacity];


//        map one array to the other via indexes indicating the pointers
    node_pt it;
    int j;
    for (i = 0; i < pool_mgr->gap_ix_capacity; i++) {
        item[i] =2147483647;
        it = pool_mgr->gap_ix[i].node;
        if (it == &pool_mgr->node_heap[i])
        {
            item[i] = i;
        }
    }


    if (((float) pool_mgr->used_nodes/ pool_mgr->total_nodes)
        > MEM_NODE_HEAP_FILL_FACTOR){

        int gapIndexTemp[pool_mgr->total_nodes];


        node_pt temp = realloc(pool_mgr->node_heap, pool_mgr->total_nodes * MEM_NODE_HEAP_EXPAND_FACTOR
                                                * sizeof(node_t));

        if (!temp)
        {
            return ALLOC_FAIL;
        }

        pool_mgr->node_heap = temp;

        //remap the memory from the array
        for (i = 0; i < pool_mgr->gap_ix_capacity; i++) {
            if (item[i] != 2147483647)
            {
                //item[i] = j;
                pool_mgr->gap_ix[i].node = &pool_mgr->node_heap[i];
            }
        }


        for (i = pool_mgr->total_nodes; i < pool_mgr->total_nodes* MEM_NODE_HEAP_EXPAND_FACTOR; i++)
        {
            pool_mgr->node_heap[i].next = NULL;
            pool_mgr->node_heap[i].prev = NULL;
            pool_mgr->node_heap[i].allocated = 0;
            pool_mgr->node_heap[i].used = 0;
            pool_mgr->node_heap[i].alloc_record.size = 0;
            pool_mgr->node_heap[i].alloc_record.mem = NULL;
        }
        pool_mgr->total_nodes = pool_mgr->total_nodes * MEM_NODE_HEAP_EXPAND_FACTOR;




        return ALLOC_OK;
    }
    else {
    return ALLOC_FAIL;
    }
}

static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr) {




    int i;
    if (((float) pool_mgr->pool.num_gaps/ pool_mgr->gap_ix_capacity)
        > MEM_GAP_IX_FILL_FACTOR){



       int item[pool_mgr->gap_ix_capacity];


//        map one array to the other via indexes indicating the pointers
        node_pt it;
        int j;
        for (i = 0; i < pool_mgr->gap_ix_capacity; i++) {
            item[i] =2147483647;
                it = pool_mgr->gap_ix[i].node;
                if (it == &pool_mgr->node_heap[i])
                {
                    item[i] = i;
                }
        }

        gap_pt temp = realloc(pool_mgr->gap_ix, pool_mgr->gap_ix_capacity * MEM_GAP_IX_EXPAND_FACTOR
                                                 * sizeof(gap_t));


        //pool_mgr->gap_ix= malloc(MEM_GAP_IX_INIT_CAPACITY*sizeof(gap_t));


        if (!temp)
        {
            return ALLOC_FAIL;
        }

        pool_mgr->gap_ix = temp; // assign memory to temp

        //remap the memory from the array
        for (i = 0; i < pool_mgr->gap_ix_capacity; i++) {
                if (item[i] != 2147483647)
                {
                    pool_mgr->gap_ix[i].node = &pool_mgr->node_heap[i];
                }
        }





        for (i = pool_mgr->gap_ix_capacity; i < pool_mgr->gap_ix_capacity * MEM_GAP_IX_EXPAND_FACTOR; i++)
        {
            pool_mgr->gap_ix[i].node = (node_pt)malloc(sizeof(node_t));
            pool_mgr->gap_ix[i].node->prev = NULL;
            pool_mgr->gap_ix[i].node->next = NULL;
            pool_mgr->gap_ix[i].node->alloc_record.size = 0;
            pool_mgr->gap_ix[i].node->alloc_record.mem= NULL;
            pool_mgr->gap_ix[i].node->used = 0;
            pool_mgr->gap_ix[i].node->allocated= 0;
            pool_mgr->gap_ix[i].size = 0;
        }
        pool_mgr->gap_ix_capacity = pool_mgr->gap_ix_capacity * MEM_GAP_IX_EXPAND_FACTOR;

        return ALLOC_OK;
    }
    else {
        return ALLOC_FAIL;
    }
}

static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr,
                                       size_t size,
                                       node_pt node) {

    // expand the gap index, if necessary (call the function)
    _mem_resize_gap_ix(pool_mgr);

    // add the entry at the end
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].node = node;

    // update metadata (num_gaps)
    pool_mgr->pool.num_gaps++;

    // sort the gap index (call the function)
    _mem_sort_gap_ix(pool_mgr);

    // check success
    int numGaps = 0;

    node_pt nodePtr = pool_mgr->node_heap;

    while (nodePtr != NULL) {
        // count the number of actual gaps
        if (nodePtr->used == 1 && nodePtr->allocated == 0) numGaps++;
        nodePtr = nodePtr->next;
    }

    if (numGaps != pool_mgr->pool.num_gaps) {
        printf("Num gaps mismatch\n");
        printf("Pool gaps: ");
        printf("%zu\n", pool_mgr->pool.num_gaps);
        printf("Counted gaps: ");
        printf("%zu\n", numGaps);
        return ALLOC_FAIL;
    }

    return ALLOC_OK;
}

static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                            size_t size,
                                            node_pt node) {
    // find the position of the node in the gap index
    // loop from there to the end of the array:
    //    pull the entries (i.e. copy over) one position up
    //    this effectively deletes the chosen node
    // update metadata (num_gaps)
    // zero out the element at position num_gaps!

    return ALLOC_FAIL;
}

// note: only called by _mem_add_to_gap_ix, which appends a single entry
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr) {
    // the new entry is at the end, so "bubble it up"
    // loop from num_gaps - 1 until but not including 0:
    //    if the size of the current entry is less than the previous (u - 1)
    //    or if the sizes are the same but the current entry points to a
    //    node with a lower address of pool allocation address (mem)
    //       swap them (by copying) (remember to use a temporary variable)

    int i = 0;
    gap_t temp;

    while (pool_mgr->gap_ix[i+1].size != 0)
    {
        i++;
    }



    while (i != 0 && pool_mgr->gap_ix[i].size < pool_mgr->gap_ix[i-1].size )
    {
        temp = pool_mgr->gap_ix[i];
        pool_mgr->gap_ix[i] = pool_mgr->gap_ix[i-1];
        pool_mgr->gap_ix[i-1] = temp;
        i--;
    }

    return ALLOC_OK;
}

static alloc_status _mem_invalidate_gap_ix(pool_mgr_pt pool_mgr) {
    return ALLOC_FAIL;
}

