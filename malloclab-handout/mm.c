/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * https://blog.csdn.net/weixin_44520881/category_10334676.html
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
    "wq",
    /* First member's full name */
    "wq",
    /* First member's email address */
    "1054588756@qq.com",
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

/* Basic constants and macros */
#define WSIZE   4   // word and header/footer size (bytes)
#define DSIZE   8
#define CHUNKSIZE (1 << 12) // Extend heap by this amount (bytes)
#define MAX(x, y) ((x) > (y)? (x): (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
/* read and write a word at address p*/
#define GET(p)      (*(unsigned *)(p))
#define PUT(p, val) (*(unsigned *)(p) = (val))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & (~0x7))
#define GET_ALLOC(p) (GET(p) & (0x1))
/* Geven block ptr bp, compute address of its header and footer*/
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/* Given block *ptr!* bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

static void *extend_heap(size_t words);
static void *caolesce(void *bp);
static void *find_fit(size_t asize);
static void place(char *bp, size_t asize);
static void mm_check();

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    mem_init();
    void *heap_listp;
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    // if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
    //     return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;   //adjust block size
    char *bp;
    if (size == 0) return NULL;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    } else {    //no fit
        if ((bp = extend_heap(asize)) == NULL)
            return NULL;
        place(bp, asize);
        return bp;
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    caolesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size) {
    if (bp == NULL) 
        return mm_malloc(size);
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }
    char *oldptr = (char *)bp;
    char *newptr;
    size_t asize;

    if (size <= DSIZE)  //+1然后向上8取整。
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    size_t old_size = GET_SIZE(HDRP(oldptr));
    if (asize == old_size) return bp;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));

    if (next_alloc == 0 && next_size >= asize - old_size) {
        //printf("next_size = %u, asize = %u, old_size = %u, 差 = %u\n",next_size,asize,old_size,asize - old_size);
        PUT(HDRP(oldptr), PACK(asize, 1));
        PUT(FTRP(oldptr), PACK(asize, 1));
        if (next_size > asize - old_size) {
            PUT(HDRP(NEXT_BLKP(oldptr)), next_size + old_size - asize);
            PUT(FTRP(NEXT_BLKP(oldptr)), next_size + old_size - asize);
        }
        return oldptr;
    } else {
        if ((newptr = mm_malloc(size)) == NULL)
            return  NULL;
        size_t copy_size = GET_SIZE(HDRP(oldptr)) - DSIZE; //头部尾部不拷贝
        memcpy(newptr, oldptr, copy_size);
        mm_free(oldptr);
        return newptr;
    }
}

static void *extend_heap(size_t asize) {
    char *bp;
    size_t extendsize;
    bp = mem_heap_hi();
    bp = bp + 1;
    if (GET_ALLOC(HDRP(PREV_BLKP(bp))) == 0)
        extendsize = asize - GET_SIZE(HDRP(PREV_BLKP(bp)));
    else
        extendsize = asize;
    if ((bp = mem_sbrk(extendsize)) == NULL) 
        return NULL;
    PUT(HDRP(bp), PACK(extendsize, 0));
    PUT(FTRP(bp), PACK(extendsize, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    bp = caolesce(bp);
    return bp;
}

/*
 * merge prev and next which is empty
*/
static void *caolesce(void *bp) {
    unsigned next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    unsigned prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    unsigned size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        //puts("prev_alloc = 1 ; next_alloc = 1");
        return bp;
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *find_fit(size_t asize) {
    char *bp = mem_heap_lo();
    bp = bp + 8;
    size_t size = GET_SIZE(HDRP(NEXT_BLKP(bp))); 
    /* ==asize也可以跳出，因为在malloc已经进行了向上取整（如果是8则向上再取8 */
    while (size != 0 && ((size = GET_SIZE(HDRP(bp))) < asize || GET_ALLOC(HDRP(bp)) == 1)) {
        bp = NEXT_BLKP(bp);//下一个节点
    }
    if (size == 0) //没有满足的
        return NULL;
    return bp;
}

static void place(char *bp, size_t asize) {
    size_t next_size = GET_SIZE(HDRP(bp)) - asize;
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    if (next_size > 0) {
        PUT(HDRP(NEXT_BLKP(bp)), next_size);
        PUT(FTRP(NEXT_BLKP(bp)), next_size);
    }
}

/* debug */
static void mm_check() {
    char *bp = mem_heap_lo();
    bp = bp + 8;    //头节点始终置为1
    size_t size;
    int no = 0;
    while ((size = GET_SIZE(HDRP(bp))) != 0) {
        printf("第%d个节点,占用情况:%d,大小:%u\n", no, GET_ALLOC(HDRP(bp)), size);
        bp = NEXT_BLKP(bp);//下一个节点
        no++;
    }
    printf("第%d个节点,占用情况:%d,大小:%u\n\n", no, GET_ALLOC(HDRP(bp)), size);
    return;
}

// void short1() {
//     mm_init();
//     void *m0 = mm_malloc(2040);mm_check();
//     void *m1 = mm_malloc(2040);mm_check();
//     mm_free(m1);mm_check();
//     void *m2 = mm_malloc(48);mm_check();
//     void *m3 = mm_malloc(4072);mm_check();
//     mm_free(m3);mm_check();
//     void *m4 = mm_malloc(4072);mm_check();
//     mm_free(m0);mm_check();
//     mm_free(m2);mm_check();
//     void *m5 = mm_malloc(4072);mm_check();
//     mm_free(m4);mm_check();
//     mm_free(m5);mm_check();
// }

// void short2() {
//     mm_init();
//     void *m1 = mm_malloc(2040);mm_check();
//     void *m2 = mm_malloc(4010);mm_check();
//     void *m3 = mm_malloc(48);mm_check();
//     void *m4 = mm_malloc(4072);mm_check();
//     void *m5 = mm_malloc(4072);mm_check();
//     void *m6 = mm_malloc(4072);mm_check();
//     mm_free(m1);mm_check();
//     mm_free(m2);mm_check();
//     mm_free(m3);mm_check();
//     mm_free(m4);mm_check();
//     mm_free(m5);mm_check();
//     mm_free(m6);mm_check();
// }

// void short3() {
//     mm_init();
//     void *m0, *m1, *m2, *m3, *m4, *m5, *m6;
//     m0 = mm_malloc(512);
//     m1 = mm_malloc(128);
//     m0 = mm_realloc(m0, 604);mm_check();
//     m2 = mm_malloc(128);
//     mm_free(m1); mm_check();
//     m0 = mm_realloc(m0, 768); mm_check();
//     m3 = mm_malloc(128);
//     mm_free(m2);
//     m0 = mm_realloc(m0, 896);
//     m4 = mm_malloc(128);
//     mm_free(m3);mm_check();
//     m0 = mm_realloc(m0, 1024);mm_check();
//     m5 = mm_malloc(128);
//     mm_free(m4);
//     m0 = mm_realloc(m0, 1152);
//     unsigned *p = (unsigned *)m0;
//     for (int i = 0; i < (1152 / 4); i++)
//        p[i] = 0xF0F0F0F0;

//     puts("");
//     m6 = mm_malloc(128);
//     mm_free(m5);
//     m0 = mm_realloc(m0, 1280);

//     p = (unsigned *)m0;
//     for (int i = 0; i < (1152 / 4); i++)
//         p[i] = printf("%x ", p[i]);
//     puts("");
// }

// void short4() {
//     mm_init();
//     void *m0, *m1, *m2;
//     m0 = mm_malloc(896);
//     m1 = mm_malloc(864);
//     mm_free(m1);mm_check();
//     m0 = mm_realloc(m0, 1024);mm_check();
// }

// int main() {
//     short3();
//     return 0;
// }
//gcc mm.c memlib.c -m32
//./mdriver -V -f short1-bal.rep
//./mdriver -V -f traces/realloc2-bal.rep
// ./mdriver -v -t traces/