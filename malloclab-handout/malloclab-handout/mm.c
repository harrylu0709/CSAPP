/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Constants and macros */
#define WSIZE 4 /* Word & header/footer size in Byte */
#define DSIZE 8 /* Double words size in Byte */
#define CHUNKSIZE (1<<12) /* Extend heap size (4K Byte) when failed to find a proper block */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* PACK size & allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* read & write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

/* get size and allocated bit of a block */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* compute address of header and footer from a given pointer of a block */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define FIRST_FIT   1
#define NEXT_FIT    0

/* compute address of next and previous block from a given pointer of a block */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)- DSIZE)))

#define PREV_PTR(bp) ((char*)bp)
#define NEXT_PTR(bp) ((char*)(bp) + WSIZE)
/* Given block ptr bp, compute address of foward and back pointer field (foward and back blocks are logically linked) */
#define GET_PREV_ADDR(bp)  (*(char **)(bp))
#define GET_NEXT_ADDR(bp)  (*(char **)(NEXT_PTR(bp)))
#define PUT_PTR(p, ptr)  (*(unsigned int **)(p) = (unsigned int*)(ptr))
#define SEGLIST_SIZE        13
#define HEAP_CHECK          0
#define INSERT_BY_SIZE      1
static char *heap_listp;
char *prev_listp;
char *exp_listp;
void *seglist[SEGLIST_SIZE];
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free_blk(void *bp);
static void remove_free_blk(void *bp);
int get_list_idx(size_t asize);
int mm_check(void);
int in_free_list(void *ptr);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /*Create initial empty heap*/
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1)
        return -1;
    
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));
    heap_listp += DSIZE;
#if NEXT_FIT
    prev_listp = heap_listp;
#endif

    /*Extend heap by CHUNKSIZE*/
    char *bp;
    if((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;

    PUT_PTR(PREV_PTR(bp), NULL);
    PUT_PTR(NEXT_PTR(bp), NULL);

    int i;
    for(i = 0; i< SEGLIST_SIZE ; i++)
    {
        seglist[i] = NULL;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    char* bp;

    /*ignore spurious request*/
    if(size == 0)
        return NULL;
    
    /*adjust size for alignment*/
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = (((size + DSIZE) + (DSIZE - 1))/DSIZE) * DSIZE; //csapp p.135 https://pse.is/6ly3e5

    /*search free list for a fit*/
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /*expand heap if no fit found*/
    size_t extend_size = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extend_size/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
#if HEAP_CHECK
    if(mm_check())
    {
        exit(1);
    }
#endif
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{   
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0)); //Header
    PUT(FTRP(bp), PACK(size, 0)); //Footer
    coalesce(bp); 
#if HEAP_CHECK
    if(mm_check())
    {
        exit(1);
    }
#endif 
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    //size = GET_SIZE(HDRP(oldptr)); //new statement
    copySize = GET_SIZE(HDRP(oldptr));//new statement
    //copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    int size;
    size = (words % 2) ? (words + 1)*WSIZE : words*WSIZE;

    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT((HDRP(bp)), PACK(size, 0)); //Header
    PUT((FTRP(bp)), PACK(size, 0)); //Footer
    PUT((HDRP(NEXT_BLKP(bp))), PACK(0, 1)); //New epilogue header

    PUT_PTR(PREV_PTR(bp), NULL);
    PUT_PTR(NEXT_PTR(bp), NULL);
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    void *prev_blk = PREV_BLKP(bp);
    void *next_blk = NEXT_BLKP(bp);

    if(prev_alloc && next_alloc)                    /* case 1 */
    {
        insert_free_blk(bp);
        return bp;
    }
    else if(prev_alloc && (!next_alloc))            /* case 2 */
    {
        size += GET_SIZE((HDRP(NEXT_BLKP(bp))));
        remove_free_blk(next_blk);
    }
    else if((!prev_alloc) && next_alloc)            /* case 3 */
    {
        size += GET_SIZE((FTRP(PREV_BLKP(bp))));
        remove_free_blk(prev_blk);
        bp = prev_blk;
    }
    else                                            /* case 4 */
    {
        size += GET_SIZE((HDRP(NEXT_BLKP(bp))));
        size += GET_SIZE((FTRP(PREV_BLKP(bp))));
        remove_free_blk(next_blk);
        remove_free_blk(prev_blk);
        bp = prev_blk;
    }
    PUT((HDRP(bp)), PACK(size, 0)); //Header
    PUT((FTRP(bp)), PACK(size, 0)); //Footer
#if NEXT_FIT
    prev_listp = bp;
#endif
    insert_free_blk(bp);
    return bp;
}

static void *find_fit(size_t asize)
{
    void *bp;
    int idx = get_list_idx(asize);
    for(; idx < SEGLIST_SIZE; idx++)
    {
        bp = seglist[idx];
        for (; bp != NULL; bp = GET_NEXT_ADDR(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize)
                return bp;
        }
    }
    return NULL;  
}
#if 0
static void *find_fit(size_t asize)
{
#if 1
#if FIRST_FIT
    char *bp  = NEXT_BLKP(heap_listp);
    size_t blk_size;
    while(1)
    {
        blk_size = GET_SIZE(HDRP(bp));
        if(GET_ALLOC(HDRP(bp)) == 1 && blk_size == 0) break;
        //if( blk_size == 0) break;

        if(GET_ALLOC(HDRP(bp)) == 0 && asize <= blk_size)
        {
            return bp;
        } 
        bp = NEXT_BLKP(bp);       
    }
    return NULL;
#endif
#if NEXT_FIT
    char *bp = prev_listp;
    
    size_t blk_size;
    while(1)
    {
        blk_size = GET_SIZE(HDRP(bp));
        if(GET_ALLOC(HDRP(bp)) == 1 && blk_size == 0) break;
        //if(blk_size == 0) break;

        if(GET_ALLOC(HDRP(bp)) == 0 && asize <= blk_size)
        {
            prev_listp = bp;
            return bp;
        } 
        bp = NEXT_BLKP(bp);       
    }
    return NULL;
#endif
#else
    size_t blk_size;
    void *bp = heap_listp;

    for (; (blk_size = GET_SIZE(HDRP(bp))) != 0; bp = NEXT_BLKP(bp)) {
        if ((GET_ALLOC(HDRP(bp)) == 0) && (blk_size >= asize))
            return bp;
    }

    return NULL;
#endif
}
#endif
static void place(void *bp, size_t asize)
{
    size_t blk_size = GET_SIZE(HDRP(bp));
    size_t split_size = blk_size - asize;
    remove_free_blk(bp); 

    if(split_size < 16)
    {
        PUT(HDRP(bp), PACK(blk_size, 1)); 
        PUT(FTRP(bp), PACK(blk_size, 1)); 
    }
    else
    {
        PUT(HDRP(bp), PACK(asize, 1)); 
        PUT(FTRP(bp), PACK(asize, 1)); 

        bp = NEXT_BLKP(bp);
        
        PUT(HDRP(bp), PACK(split_size, 0)); 
        PUT(FTRP(bp), PACK(split_size, 0)); 
        insert_free_blk(bp);
    }

#if NEXT_FIT
    prev_listp = bp;
#endif
}


static void insert_free_blk(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int idx = get_list_idx(size);
    void *current = seglist[idx];
    if(seglist[idx] == NULL)
    {
        PUT_PTR(PREV_PTR(bp), NULL);
        PUT_PTR(NEXT_PTR(bp), NULL);
        seglist[idx] = bp;
    }
    else if(seglist[idx] != NULL)
    {
#if INSERT_BY_SIZE
        while(current)  
        {
            if(GET_NEXT_ADDR(current) == NULL)
            {
                PUT_PTR(PREV_PTR(bp), current);
                PUT_PTR(NEXT_PTR(bp), NULL);
                PUT_PTR(NEXT_PTR(current),bp);
                break;
            } 
            size_t curr_size = GET_SIZE(HDRP(current));
            size_t next_size = GET_SIZE(HDRP(GET_NEXT_ADDR(current)));
            if(curr_size <= size && size <= next_size)
            {
                void *next_blk = GET_NEXT_ADDR(current);
                PUT_PTR(PREV_PTR(bp), current);
                PUT_PTR(NEXT_PTR(bp), next_blk);
                PUT_PTR(PREV_PTR(next_blk), bp);
                PUT_PTR(NEXT_PTR(current), bp);
                break;
            }     
            current = GET_NEXT_ADDR(current);
        }
#else
        while(current)
        {
            if(GET_NEXT_ADDR(current) == NULL)
            {
                PUT_PTR(PREV_PTR(bp), current);
                PUT_PTR(NEXT_PTR(bp), NULL);
                PUT_PTR(NEXT_PTR(current),bp);
                break;
            }      
            current = GET_NEXT_ADDR(current);      
        }
#endif
    }
}

static void remove_free_blk(void *bp)
{
    void *prev_blk = GET_PREV_ADDR(bp);
    void *next_blk = GET_NEXT_ADDR(bp);
    size_t size = GET_SIZE(HDRP(bp));
    int idx = get_list_idx(size);


    if(!prev_blk && !next_blk)
    {
        seglist[idx] = NULL;
    }
    else if(!prev_blk && next_blk)
    {
       PUT_PTR(PREV_PTR(next_blk), NULL);
       seglist[idx] = next_blk;
    }   
    else if(prev_blk && !next_blk)
    {
       PUT_PTR(NEXT_PTR(prev_blk), NULL);
    }
    else
    {
       PUT_PTR(PREV_PTR(next_blk), prev_blk);
       PUT_PTR(NEXT_PTR(prev_blk), next_blk);
    }
}

int get_list_idx(size_t asize)
{
    int idx = 0;
    while(asize > 1 && idx < (SEGLIST_SIZE - 1))
    {
        asize >>= 1;
        idx++;
    }
    return idx;
}

int mm_check(void)
{
    void *ptr = NEXT_BLKP(heap_listp);

    while(GET_SIZE(HDRP(ptr)) != 0)
    {
        if(GET_ALLOC(HDRP(ptr)) == 0)
        {
            if(in_free_list(ptr) == 0)
            {
                printf("%p not in free list size %d\n",ptr, GET_SIZE(HDRP(ptr)));
                return 1;
            }

            if(GET_ALLOC(HDRP(PREV_BLKP(ptr))) == 0 || GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0)
            {
                printf("%p contiguous free block\n",ptr);
                return 1;
            }

            if(GET_PREV_ADDR(ptr) != NULL)
            {
                if(GET_NEXT_ADDR(GET_PREV_ADDR(ptr)) != ptr)
                {
                    printf("The next's previous isn't this\n");
                    return 1;
                }
            }

            if(GET_NEXT_ADDR(ptr) != NULL)
            {
                if(GET_PREV_ADDR(GET_NEXT_ADDR(ptr)) != ptr)
                {
                    printf("The prev's next isn't this\n");
                    return 1;
                }
            }
        }
        else
        {
            if(in_free_list(ptr))
            {
                printf("%p is allocated but is in free list\n",ptr);
                return 1;
            }
            if(ptr!=heap_listp && FTRP(PREV_BLKP(ptr)) + WSIZE > HDRP(ptr))
            {
                printf("%p is overlap\n", ptr);
                printf("prev footer: %p, current header: %p\n",FTRP(PREV_BLKP(ptr)), HDRP(ptr));
                return 1;
            }
        }

        ptr = NEXT_BLKP(ptr);
    }
    return 0;
}

int in_free_list(void *ptr)
{
    void *current;
    int i;
    for(i = 0; i< SEGLIST_SIZE; i++)
    {
        current = seglist[i];
        while(current)
        {
            if(GET_ALLOC(HDRP(current)) != 0)
            {
                printf("%p is invalid free block\n",current);
                return 0;
            }
            if(current == ptr)
            {
                return 1;
            }
            current = GET_NEXT_ADDR(current);
        }
    }
    return 0;
}





