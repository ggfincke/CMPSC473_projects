/*
 * mm.c
 *
 * Name: Avanish Grampurohit & Garrett Fincke 
 *
 * This code does something I guess, idk
 *
 *
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */


/*Globl variables*/
static int const MAX_LEN = 61;
char *heap_listp;
void *seg_list[61]; 

/* What is the correct alignment? */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
  return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/* Basic constants code taken from CSAPP page 882 */
const size_t WSIZE =  8; /* Word and header/footer size (bytes) */
const size_t DSIZE =  16; /* Double word size (bytes) */
const size_t CHUNKSIZE = (1<<14); /* Extend heap by this amount (bytes) */


/*Basic funtions. (Refered from CSAPP page 882.) */
static uint64_t MAX(uint64_t x, uint64_t y)
{
    return (x > y) ? x : y;
}

/* Pack a size and allocated bit into a word */
static uint64_t PACK(uint64_t size, uint64_t alloc)
{
  return (size) | (alloc);
}

/*Read and write a word at address p*/
static uint64_t GET(void *p)
{
  return *(uint64_t *) p;
}
static void PUT(void *p, size_t val)
{
  *(uint64_t*)p = val;
}

/* Read the size and allocated fields from address p */
static size_t GET_SIZE(void *p) {
  return ((GET(p)) & ~0x7);
}
static size_t GET_ALLOC(void *p) {
  return ((GET(p)) & 0x1);
}

//get tag (2nd bit)
static size_t GET_TAG(void *p)
{
  return (GET(p) & 0x2);
}

//put tag (2nd bit)
static void PUT_TAG(void *p, size_t val)
{
  *(uint64_t*)p = val | GET_TAG(p);
}

//add tag using or 
static void TAG_ADD (void *p)
{
  *(uint64_t*)p = GET(p) | 0x2;
}

//remove tag using ampersand
static void TAG_REMOVE (void *p)
{
  *(uint64_t*)p = GET(p) & ~0x2; 
}
/* Given block ptr bp, compute address of its header and footer */
static void* HDRP(void* bp) {
    return (char*)bp - WSIZE;
}
static void* FTRP(void* bp) {
    return (char*)bp + GET_SIZE(HDRP(bp)) - DSIZE;
}

/* Given block ptr bp, compute address of next and previous blocks */
static void* NEXT_BLKP(void* bp) {
    return (char*)bp + GET_SIZE((char*)bp - WSIZE);
}
static void* PREV_BLKP(void* bp) {
    return (char*)bp - GET_SIZE((char*)bp - DSIZE);
}

//Get previous and next pointer in free list.
static void* NEXT_PTR(void* bp){
  return (char *)(bp + WSIZE);
}
static void* PREV_PTR(void* bp){
  return ((char *)(bp));
}
//address of block in seglist
static void* NEXT_BLK(void *bp){
  return (*(char**)(NEXT_PTR(bp)));
}
static void* PREV_BLK(void *bp){
  return (*(char**)(bp));
}

//set ptr (used for next/prev)
static void SET_PTR(void *p, void* ptr)
{
  *(uint64_t*)p = (uint64_t)ptr;
}

//function to select list number. 
static int select_list(size_t size){
  int list = 0;
  if (size == 0){return -1;}
  while (size >>= 1){
    list++;
  }
  if (list > (MAX_LEN - 1)){
    return (MAX_LEN - 1);
  }
  return list;
}

//add node to seg_list
static void node_add(void *ptr, size_t size){
  int list = select_list(size);
  void *curr_list = seg_list[list];
  void *insert_loc = NULL;

  //find location to insert.
  while((curr_list != NULL) && (size > GET_SIZE(HDRP(curr_list)))){
    insert_loc = curr_list;
    curr_list = PREV_BLK(curr_list);
  }

  //find location to insert block based on the two block;
  if (curr_list)
    {
      if (insert_loc)
	{
	  SET_PTR(PREV_PTR(ptr), curr_list);
	  SET_PTR(NEXT_PTR(curr_list), ptr);
	  SET_PTR(PREV_PTR(insert_loc), ptr);
	  SET_PTR(NEXT_PTR(ptr), insert_loc);
	}
      else
	{
	  SET_PTR(PREV_PTR(ptr), curr_list);
	  SET_PTR(NEXT_PTR(curr_list), ptr);
	  SET_PTR(NEXT_PTR(ptr), NULL);
	  seg_list[list] = ptr;
	}
    }
  else
    {
      if (insert_loc)
	{
	  SET_PTR(PREV_PTR(ptr), NULL);
	  SET_PTR(NEXT_PTR(ptr), insert_loc);
	  SET_PTR(PREV_PTR(insert_loc), ptr);
	}
      else
	{
	  SET_PTR(PREV_PTR(ptr), NULL);
	  SET_PTR(NEXT_PTR(ptr), NULL);
	  seg_list[list] = ptr;
	}
    }
}

//remove node from seg_list
static void node_remove(void *ptr)
{
  size_t size = GET_SIZE(HDRP(ptr));
  //find location in seglist according to size.
  int list = select_list(size);
  
  //remove node according to the blocks and adjust previous and next ptrs.
  if (PREV_BLK(ptr) != NULL)
    {
      if(NEXT_BLK(ptr) != NULL)
	{
	  SET_PTR(NEXT_PTR(PREV_BLK(ptr)), NEXT_BLK(ptr));
	  SET_PTR(PREV_PTR(NEXT_BLK(ptr)), PREV_BLK(ptr));
	}
      else
	{
	  SET_PTR(NEXT_PTR(PREV_BLK(ptr)), NULL);
	  seg_list[list] = PREV_BLK(ptr);
	}
    }
    else
      {
	if (NEXT_BLK(ptr) != NULL)
	  {
	    SET_PTR(PREV_PTR(NEXT_BLK(ptr)), NULL);
	  }
	else
	  {
	    seg_list[list] = NULL;
	  }
      }

}

/*Refered CSAPP, page 885 */
static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  
  if (prev_alloc && next_alloc) { /* Case 1 */
    return bp;
   }

  if (prev_alloc && !next_alloc) { /* Case 2 */
    node_remove(bp);
    node_remove(NEXT_BLKP(bp));/*adjusting blocks in free list*/
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size,0));
   }

  else if (!prev_alloc && next_alloc) { /* Case 3 */
    node_remove(bp);
    node_remove(PREV_BLKP(bp));/*adjusting blocks in free list*/
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
   }

  else { /* Case 4 */
    node_remove(bp);
    node_remove(PREV_BLKP(bp));
    node_remove(NEXT_BLKP(bp));/*adjusting blocks in free list*/
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
   }

  //insert in seg list
  node_add(bp, size);
  
  return bp;
}

/*Function to extend the heap to CHUNCKSIZE. Reference CSAPP page 883.*/
static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;

  size = (words % 2) ? (words + 1) *WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
    {
      return NULL;
    }
 
  PUT(HDRP(bp), PACK(size,0));
  PUT(FTRP(bp),  PACK(size,0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); 
  
  node_add(bp, size);
  return coalesce(bp);
}

//finds if there is a fit for
static void *find_fit(size_t asize)
{
    int list_start = select_list(asize);
    void *list_node;

    for (int list = list_start; list < MAX_LEN; list++)
    {
        list_node = seg_list[list];
        while (list_node != NULL) 
        {
            if (asize <= GET_SIZE(HDRP(list_node))) 
            {
                // Found a fit
                return list_node;
            }
            list_node = PREV_BLK(list_node);
        }
    }
    return NULL;
}

//set hdrs and ftrs for new blocks, split if enough space is remaining 
static void place(void *bp, size_t asize)
{
  size_t psize = GET_SIZE(HDRP(bp));
  size_t rem = psize - asize;
  
  node_remove(bp);
  if (rem <= DSIZE * 2)
    {
      PUT(HDRP(bp), PACK(psize, 1));
      PUT(FTRP(bp), PACK(psize, 1));
    }
  else
    {
      PUT(HDRP(bp), PACK(asize, 1));
      PUT(FTRP(bp), PACK(asize, 1));
      PUT(HDRP(NEXT_BLKP(bp)), PACK(rem, 0));
      PUT(FTRP(NEXT_BLKP(bp)), PACK(rem, 0));
      node_add(NEXT_BLKP(bp), rem);
    }
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
  int list;
  
  for (list = 0; list < MAX_LEN; list++)
    {
      seg_list[list] = NULL;
    }

  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
  {
    return false;
  }
  PUT(heap_listp, 0); /* Alignment padding */
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); /* Prologue header */
  PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); /* Prologue footer */
  PUT(heap_listp + (3*WSIZE), PACK(0,1)); /* Epilogue header */
  heap_listp += (2*WSIZE);
  
   /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
  {
    return false;
  }
  //Inital block represents entire heap and is only block in explicit free list 
  return true;
}

/*
 * malloc - alloc block, return ptr to region 
 */
void* malloc(size_t size)
{
  size_t asize;                           
  size_t extendsize;                      
  void *bp;

  if (size == 0)
    return NULL;

  if (size <= DSIZE)
    {    
      asize = 2 * DSIZE;
    }  
  else
    {
      asize = align(size + ALIGNMENT);
    }   
  
  bp = find_fit(asize);
  if (bp == NULL)
    {
      extendsize = MAX(asize, CHUNKSIZE);
      if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
	{
	  return NULL;
	}
    }
  
  place(bp, asize);
  return bp;
}

/*
 * free - frees block pointed to by ptr
 */
void free(void* bp)
{
  size_t size = GET_SIZE(HDRP(bp));
  TAG_REMOVE(HDRP(NEXT_BLKP(bp)));
  PUT_TAG(HDRP(bp), PACK(size, 0));
  PUT_TAG(FTRP(bp), PACK(size, 0));
  node_add(bp, size);
  coalesce(bp);
}

/*
 * realloc
 */
//rellocate block, extend heap if needed. Padded w/ buffer, assuming block is expanded by constant bytes per realloc. 
void* realloc(void* oldptr, size_t size)
{
  if (size == 0){
    mm_free(oldptr);
    return NULL;
  }
  
  if(oldptr == NULL){
    return NULL;
  }
  //assign and compare old size and aligned size
  size_t oldsize = GET_SIZE(HDRP(oldptr));
  size_t asize;
  if(size <= DSIZE)
    {
      asize = 2*DSIZE;
    }
  
  else
    {
      asize = align(size);
    }
  
  oldsize -= ALIGNMENT;
  if (oldsize >= asize){
    return oldptr;
  }
  //allocate a new block and copy old ptr
  void *newptr = mm_malloc(size);

  if (asize < oldsize){
    oldsize = asize;
  }
  memcpy(newptr, oldptr, oldsize);
  mm_free(oldptr);
  return newptr;
}




//heapchecker needs implemented
/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap - checks heap for correctness
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
  //Check prologue and epilogue blocks
  if (GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp))) {
    printf("Line %d: Error: Bad prologue header\n", lineno);
    return false;
  }
  
  //Checks footer
  if (GET_SIZE(FTRP(heap_listp)) != DSIZE || !GET_ALLOC(FTRP(heap_listp))) {
    printf("Line %d: Error: Bad prologue footer\n", lineno);
    return false;
  }
  
  //
  if (GET_SIZE(HDRP(NEXT_BLKP(heap_listp))) != 0 || !GET_ALLOC(HDRP(NEXT_BLKP(heap_listp)))) {
    printf("Line %d: Error: Bad epilogue header\n", lineno);
    return false;
  }
  
    //Check each block in the free list
  for (int list = 0; list < MAX_LEN; list++) {
    void *block = seg_list[list];
    while (block != NULL) {
      //Check alignment
      if (!aligned(block)) {
	printf("Line %d: Error: Block not aligned\n", lineno);
	return false;
      }

      //Check if block is in the heap
      if (!in_heap(block)) {
	printf("Line %d: Error: Block not in heap\n", lineno);
	return false;
      }
      
      //Check block's header and footer
      if (GET_SIZE(HDRP(block)) != GET_SIZE(FTRP(block))) {
	printf("Line %d: Error: Header and footer size mismatch\n", lineno);
	return false;
      }
      
      //Check if block allocated in free list
      if (GET_ALLOC(HDRP(block))) {
	printf("Line %d: Error: Allocated block in free list\n", lineno);
	return false;
      }
      
      //Check coalescing
      if (!GET_ALLOC(HDRP(NEXT_BLKP(block))) && !GET_ALLOC(HDRP(block))) {
	printf("Line %d: Error: Coalescing failed\n", lineno);
	return false;
      }
      
      //Check tags
      if (GET_TAG(HDRP(block)) != 0) {
	printf("Line %d: Error: Block has tag\n", lineno);
	return false;
      }
      
      block = PREV_BLK(block);
    }
  }
#endif /* DEBUG */
  return true;  
}
