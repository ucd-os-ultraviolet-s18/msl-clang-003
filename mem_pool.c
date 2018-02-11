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

// FOR DEBUGGING PURPOSES ONLY
void nodeReport(pool_pt pool) { /*

    pool_mgr_pt managerPtr = ((pool_mgr_pt)pool);

    printf("---------------------------\n");
    printf("Node Report\n");
    printf("---------------------------\n");

    node_pt nodePtr = managerPtr->node_heap;
    while (nodePtr != NULL) {
        printf("Address: ");
        printf("%p\n", nodePtr);
        printf("Is used: ");
        printf("%u\n", nodePtr->used);
        printf("Is allocated: ");
        printf("%u\n", nodePtr->allocated);
        printf("Space allocated: ");
        printf("%d\n", (int)nodePtr->alloc_record.size);
        printf("Prev node: ");
        printf("%p\n", nodePtr->prev);
        printf("Next node: ");
        printf("%p\n", nodePtr->next);
        printf("---------------------------\n");
        nodePtr = nodePtr->next;
    } */

}

void gapReport(pool_mgr_pt managerPtr) {

    /*
    printf("***************************\n");
    printf("Gap Report\n");
    printf("***************************\n");
    for (int i = 0; i < managerPtr->pool.num_gaps; i++) {
        printf("Address: ");
        printf("%zp\n", managerPtr->gap_ix[i].node);
        printf("Size: ");
        printf("%zu\n", managerPtr->gap_ix[i].size);
        printf("***************************\n");
    }*/
}



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

    printf("--------------------------\n");
    printf("Allocating new memory pool\n");
    printf("--------------------------\n");

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt managerPtr = ((pool_mgr_pt)pool);

    // check if any gaps, return null if none
    if (pool->num_gaps == 0) {
        printf("No gaps available!\n");
        return NULL;
    }

    // resize node heap if too small
    _mem_resize_node_heap(managerPtr);

    // points to gap node where the new allocation will go
    node_pt nodeToReplace = NULL;

    // gapsize is difference between alloc size and gap size
    size_t gapSize = 0;

    // if BEST_FIT:
    // traverse through gap_ix array and look for first spot that the node
    // will fit in
    if (pool->policy == BEST_FIT) {

        // this checks the gap index for the first size
        int i = 0;
        while (managerPtr->gap_ix[i].size < size && i < pool->num_gaps) i++;

        // if the element we end on isn't big enough
        if (managerPtr->gap_ix[i].size < size) {
            printf("No room for node!\n");
            return NULL;
        }

        else { // set the node found as the node to replace and remove from gap ix
            nodeToReplace = managerPtr->gap_ix[i].node;
            gapSize = managerPtr->gap_ix[i].node->alloc_record.size - size;

            _mem_remove_from_gap_ix(managerPtr,
                                    managerPtr->gap_ix[i].node->alloc_record.size,
                                    managerPtr->gap_ix[i].node);
        }
    }

    else if (pool->policy == FIRST_FIT) {

        nodeToReplace = managerPtr->node_heap; // set ptr to point at first element in node heap

        // traverse until we find a node that isn't used and isn't too small
        while (nodeToReplace->allocated == 1 || nodeToReplace->alloc_record.size < size)
            nodeToReplace = nodeToReplace->next;

        if (nodeToReplace == NULL) {
            printf("No room for node!\n");
            return NULL;
        }

        // set new gapsize if there is one, nodeToReplace is already at the correct address
        gapSize = nodeToReplace->alloc_record.size - size;


        // after this line, k will be index of gap pointer to get rid of
        int k = 0;
        while (nodeToReplace != managerPtr->gap_ix[k].node && k < pool->num_gaps) k++;

        // see if where we stopped is correct
        if (nodeToReplace != managerPtr->gap_ix[k].node) {
            printf("Could not find node in gap index");
            return NULL;
        }

        // remove from the gap index
        _mem_remove_from_gap_ix(managerPtr, nodeToReplace->alloc_record.size, managerPtr->gap_ix[k].node);
    }

    // now that we've found the node and gotten rid of it out of the gap_ix
    // we'll deal with the extra gaps

    // if gap size is bigger than zero, add a gap node
    if (gapSize > 0) {

        // look for next available node to hold the gap
        int j = 0;
        while (managerPtr->node_heap[j].used == 1) j++;

        // if there's no more nodes available
        if (managerPtr->node_heap[j].used == 1) {
            printf("No more nodes available!\n");
            return NULL;
        }

        node_pt newGapPtr = &(managerPtr->node_heap[j]);

        // sets new gap's attributes
        newGapPtr->used = 1;
        newGapPtr->alloc_record.size = gapSize;
        newGapPtr->next = nodeToReplace->next;

        // set the gap to go after the allocated node
        // newnode prev should still be fine

        nodeToReplace->next = newGapPtr;

        // set gap node after new node
        newGapPtr->prev = nodeToReplace;

        // add the new (smaller) gap to the gap index
        _mem_add_to_gap_ix(managerPtr, gapSize, newGapPtr);

        managerPtr->used_nodes++;
    }

    // do not need to reset newNode pointers if it takes up the entire allocation

    // set newNode attributes:
    nodeToReplace->alloc_record.size = size;
    nodeToReplace->used = 1;
    nodeToReplace->allocated = 1;

    // set pool attributes
    pool->num_allocs++;
    pool->alloc_size = pool->alloc_size + size;

    /*
    printf("Address: ");
    printf("%p\n", nodeToReplace);
    printf("Is used: ");
    printf("%u\n", nodeToReplace->used);
    printf("Is allocated: ");
    printf("%u\n", nodeToReplace->allocated);
    printf("Space allocated: ");
    printf("%d\n", (int)nodeToReplace->alloc_record.size);
    printf("Prev node: ");
    printf("%p\n", nodeToReplace->prev);
    printf("Next node: ");
    printf("%p\n", nodeToReplace->next);
    printf("---------------------------\n");
     */

    // return the user-requested memory
    return ((alloc_pt)nodeToReplace);
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


// This function deallocates the given allocation from the given memory pool
alloc_status mem_del_alloc(pool_pt pool, void * alloc) {

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt managerPtr = ((pool_mgr_pt)pool);

    // get node from alloc by casting the pointer to (node_pt)
    node_pt deletePtr = ((node_pt)alloc);

    // find the node in the node heap
    // this is node-to-delete
    node_pt nodePtr = managerPtr->node_heap; // point at head of linked list

    while (nodePtr && nodePtr != deletePtr) nodePtr = nodePtr->next;
    // traverse to find node
    // if we've gone to the end of the list and not found it
    if (nodePtr == NULL) {
        printf("Node to delete not found in memory pool\n");
        return ALLOC_FAIL;
    }

    // convert to gap node
    nodePtr->allocated = 0;
    nodePtr->alloc_record.mem = NULL;

    // do this later:
    // add gap to gap index
    _mem_add_to_gap_ix(managerPtr, nodePtr->alloc_record.size, nodePtr);

    // update metadata (num_allocs, alloc_size)
    pool->num_allocs--;
    pool->alloc_size = pool->alloc_size - nodePtr->alloc_record.size;

    // if previous node is a gap and current node is not the head
    // then just move the pointer up one node
    // then next block will take over with merging
    if (nodePtr->prev != NULL && nodePtr->prev->allocated == 0) {
        printf("Gap before node, pointing to prevNode: ");
        printf("%p\n", nodePtr->prev);
        nodePtr = nodePtr->prev;

        // this is set for below when we check for success
        deletePtr = nodePtr;
    }

    // if the next node in the list is also a gap, merge into node-to-delete
    if (nodePtr->next != NULL && nodePtr->next->allocated == 0) {

        printf("Gap after node\n");

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
            return ALLOC_FAIL; // ideally this should never happen
        }

        // update old gapnode as unused
        extraGap->alloc_record.size = 0;
        extraGap->alloc_record.mem = NULL; // should already be null but eh
        extraGap->allocated = 0;
        extraGap->used = 0;
        extraGap->prev = NULL;

        // connects the new gap to the node after the merged gap
        nodePtr->next = extraGap->next;

        // now extraGap node points to nothing
        extraGap->next = NULL;

        // set next node to point at correct thing
        if (nodePtr->next != NULL)
            nodePtr->next->prev = nodePtr;

    }

    // this block is required for if there is a gap at the beginning as well as the end
    // literally just a copy of the block above
    // an ugly way to do it
    // but it saves me writing different code for a post-node deletion
    if (nodePtr->next != NULL && nodePtr->next->allocated == 0) {

        printf("Gap after node\n");

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
            return ALLOC_FAIL; // ideally this should never happen
        }

        // update old gapnode as unused
        extraGap->alloc_record.size = 0;
        extraGap->alloc_record.mem = NULL; // should already be null but eh
        extraGap->allocated = 0;
        extraGap->used = 0;
        extraGap->prev = NULL;

        // connects the new gap to the node after the merged gap
        nodePtr->next = extraGap->next;

        // now extraGap node points to nothing
        extraGap->next = NULL;

        // set next node to point at correct thing
        if (nodePtr->next != NULL)
            nodePtr->next->prev = nodePtr;

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
    while (nodePtr != deletePtr && nodePtr != NULL) nodePtr = nodePtr->next;
    if (nodePtr == NULL) {
        printf("Cannot find gap node after deallocation");
        return ALLOC_FAIL;
    }


    // _mem_remove_from_gap_ix() already increments the number of gaps
    //if ( managerPtr->pool.num_gaps == 0)//managerPtr->pool.alloc_size == 0 &&
        //managerPtr->pool.num_gaps++;


    return ALLOC_OK;


}


void mem_inspect_pool(pool_pt pool,
                      pool_segment_pt *segments,
                      unsigned *num_segments) {

    int i;
    int j;
    pool_mgr_pt manager = ((pool_mgr_pt)pool);

// get the mgr from the pool
    // allocate the segments array with size == used_nodes

    //segments = malloc(sizeof(pool_segment_pt));
    *segments = malloc(manager->used_nodes* sizeof(pool_segment_pt));
    assert(segments != NULL);

    // check successful
    // loop through the node heap and the segments array

    for (i = 0; i <manager->used_nodes; i++){
        //(*segments)[i] = malloc(sizeof(pool_segment_t));
        (*segments)[i].size = manager->node_heap[i].alloc_record.size;
        (*segments)[i].allocated = manager->node_heap[i].allocated;
    }

    *num_segments = manager->used_nodes;
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

    printf("Used nodes: ");
    printf("%u\n", pool_mgr->used_nodes);

    printf("Total nodes: ");
    printf("%u\n", pool_mgr->total_nodes);

    if (((float) pool_mgr->used_nodes/ pool_mgr->total_nodes)
        > MEM_NODE_HEAP_FILL_FACTOR){

        printf("******Resizing node heap*********\n");

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
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].size = size;

    // update metadata (num_gaps)
    pool_mgr->pool.num_gaps++;

    // sort the gap index (call the function)
    _mem_sort_gap_ix(pool_mgr);

    //printf("Added gap\n");
    gapReport(pool_mgr);

    return ALLOC_OK;
}

static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                            size_t size,
                                            node_pt node) {

    // find the position of the node in the gap index
    int i = 0;
    while (pool_mgr->gap_ix[i].node && pool_mgr->gap_ix[i].node != node) i++;

    // loop from there to the end of the array:
    //    pull the entries (i.e. copy over) one position up
    //    this effectively deletes the chosen node
    for (int j = i; j < pool_mgr->pool.num_gaps; j++) {
        pool_mgr->gap_ix[j].node = pool_mgr->gap_ix[j+1].node;
        pool_mgr->gap_ix[j].size = pool_mgr->gap_ix[j+1].size;
    }

    // zero out the last element which is just a copy of the second-to-last
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].node = NULL;
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].size = 0;

    // update metadata (num_gaps)
    pool_mgr->pool.num_gaps--;

    _mem_sort_gap_ix(pool_mgr);

    //printf("Removed gap\n");
    gapReport(pool_mgr);

    return ALLOC_OK;
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