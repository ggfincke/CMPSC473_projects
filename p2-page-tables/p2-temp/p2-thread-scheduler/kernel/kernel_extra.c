/*
 * kernel_extra.c - Project 2 Extra Credit, CMPSC 473
 * Copyright 2023 Ruslan Nikolaev <rnikola@psu.edu>
 * Distribution, modification, or usage without explicit author's permission
 * is not allowed.
 */

#include <malloc.h>
#include <types.h>
#include <string.h>
#include <printf.h>

// Your mm_init(), malloc(), free() code from mm.c here
// You can only use mem_sbrk(), mem_heap_lo(), mem_heap_hi() and
// Project 2's kernel headers provided in 'include' such
// as memset and memcpy.
// No other files from Project 1 are allowed!

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

bool mm_init()
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

void *malloc(size_t size)
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

void free(void *ptr)
{
  size_t size = GET_SIZE(HDRP(ptr));
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  node_add(ptr, size);
  coalesce(ptr);
}
