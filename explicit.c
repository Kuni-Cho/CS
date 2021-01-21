#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
#define WSIZE 4             // 1 ?õå?ìú ?Å¨Í∏?
#define DSIZE 8             // 2 ?õå?ìú ?Å¨Í∏?
#define CHUNKSIZE (1 << 12) //?†ÑÏ≤? ?ã®?úÑ ?Ç¨?ù¥Ï¶? 2^12 (?éò?ù¥Ïß? ?Å¨Í∏?)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))       // | ?ó∞?Ç∞?ûêÎ°?, ?Å¨Í∏∞Ï?? ?ï†?ãπ ÎπÑÌä∏Î•? ?ï©Ï≥êÏÑú header, footer?óê ?Ñ£?äî?ã§.
#define GET(p) (*(unsigned int *)(p))              //?è¨?ù∏?Ñ∞?ùò Í∞íÏùÑ Í∞??†∏?ò®?ã§.
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // ?ï¥?ãπ ?è¨?ù∏?Ñ∞?óê Í∞íÏùÑ ?ûÖ?†•?ïú?ã§.
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp)-WSIZE) // ?òÑ?û¨ ?è¨?ù∏?Ñ∞?óê?Ñú 1?õå?ìú ?Å¨Í∏? ?†Ñ?úºÎ°? ?èå?ïÑÍ∞?Î©? ?ó§?ìú
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) //footer?óê?Ñú sizeÎ•? Í∞??†∏?ò®?ã§.
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))
#define PREV_FREEBLKP(bp) ((char *)(bp))
#define NEXT_FREEBLKP(bp) ((char *)(bp) + WSIZE)

static char *head_list_ptr = NULL;
static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *place(void *bp, size_t asize);
void mm_free(void *bp);
void *mm_malloc(size_t size);
int mm_init(void);
void *mm_realloc(void *ptr, size_t size);
void free_delete(void *ptr);
void detach_free(void *new_ptr);

team_t team = {
    /* Team name */
    "Explicit",
    /* First member's full name */
    "CHO JEONG HUN",
    /* First member's email address */
    "NULL",
    /* Second member's full name (leave blank if none) */
    "NULL",
    /* Second member's email address (leave blank if none) */
    "NULL"};
//?îÑÎ°§Î°úÍ∑? ?ó§?çî ?í§ Ï£ºÏÜåÎ•? Í∞?Î¶¨ÌÇ®?ã§.

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(24)) == (void *)-1) // ÏµúÏ¥à ÏµúÏÜå ?Å¨Í∏? Ïß??†ï
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(WSIZE * 4, 1));
    PUT(heap_listp + (2 * WSIZE), NULL); //PRED
    PUT(heap_listp + (3 * WSIZE), NULL); // SUCC
    PUT(heap_listp + (4 * WSIZE), PACK(WSIZE * 4, 1));
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);
    head_list_ptr = heap_listp;
    printf("extend_heap_init\n");
    if (extend_heap(CHUNKSIZE / DSIZE) == NULL) // ?ûô ?ôï?û• ?ã§?å®
        return -1;
    return 0; // ?ûô ?ôï?û• ?Ñ±Í≥?
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    // size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    size = words * DSIZE;

    printf("extend_heap_words : %d\n", words);
    printf("extend_heap_size : %d\n", size);

    // ?ã†Í∑? Í≥µÍ∞Ñ ?Éù?Ñ±?ù¥ ?ã§?å®
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // ?ã†Í∑? Í≥µÍ∞Ñ?ù¥ ?Éù?Ñ±?ù¥ ?êú Í≤ΩÏö∞
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    if (size == 0)
    {
        return NULL;
    }
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    printf("extend_heap_malloc\n");
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *find_fit(size_t asize)
{
    void *bp = head_list_ptr;
    for (bp; !GET_ALLOC(HDRP(bp)); bp = GET(NEXT_FREEBLKP(bp)))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }
    }

    return NULL;
}

static void *place(void *bp, size_t asize)
{
    free_delete(bp);
    size_t tmp_result = GET_SIZE(HDRP(bp)) - asize;
    // ?Ç®??? Í≥µÍ∞Ñ?ù¥ ÏµúÏÜå Î∏îÎ°ù ?Å¨Í∏∞Î≥¥?ã§ ?Å∞ Í≤ΩÏö∞, ?Ç®??? Í≥µÍ∞Ñ?ùÑ Î∂ÑÌï†?ï¥ Ï§??ã§.
    if (tmp_result >= 2 * DSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);

        // Ï±ÑÏõåÏß?Í≥? ?Ç®??? Î∏îÎü≠?ùÑ ?ï†?ãπ?êòÏß? ?ïä??? ?ÉÅ?ÉúÎ°? Ï¥àÍ∏∞?ôî ?ãú?Ç®?ã§.
        PUT(HDRP(bp), PACK(tmp_result, 0));
        PUT(FTRP(bp), PACK(tmp_result, 0));

        // Í∞??ö© ?ÉÅ?ÉúÍ∞? ?êòÎØ?Î°?, Í∞??ö©Î¶¨Ïä§?ä∏?óê ?ù¥?ñ¥Ï§??ã§.
        detach_free(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
    }
    else if (prev_alloc && !next_alloc)
    {
        free_delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        free_delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else // ?ïû ?í§ Î∏îÎ°ù?ù¥ Î™®Îëê ÎØ∏Ìï†?ãπ?ùº Í≤ΩÏö∞,
    {
        free_delete(PREV_BLKP(bp));
        free_delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    detach_free(bp);
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0)); // ?ï†?ãπ Í∞? Î∞òÌôò?ïòÍ∏?
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void free_delete(void *ptr)
{
    // 1. Í∞??ö© Î¶¨Ïä§?ä∏?ùò Îß? Ï≤òÏùå?ù∏ Í≤ΩÏö∞
    if (ptr == head_list_ptr)
    {
        PUT(PREV_FREEBLKP(GET(NEXT_FREEBLKP(ptr))), NULL);
        head_list_ptr = GET(NEXT_FREEBLKP(ptr));
    }
    else
    // 2. Í∞??ö© Î¶¨Ïä§?ä∏?ùò Ï§ëÍ∞Ñ?ù∏ Í≤ΩÏö∞
    {
        PUT(NEXT_FREEBLKP(GET(PREV_FREEBLKP(ptr))), GET(NEXT_FREEBLKP(ptr)));
        PUT(PREV_FREEBLKP(GET(NEXT_FREEBLKP(ptr))), GET(PREV_FREEBLKP(ptr)));
    }
}

void detach_free(void *new_ptr)
{
    char *old_ptr = head_list_ptr;
    head_list_ptr = new_ptr;
    PUT(NEXT_FREEBLKP(new_ptr), old_ptr);
    PUT(PREV_FREEBLKP(old_ptr), new_ptr);
    PUT(PREV_FREEBLKP(new_ptr), NULL);
}

/*

explicit

1. free deleteø°º≠ «ˆ¿Á≥ÎµÂ ¡¶ø‹«œ∞Ì ¥Ÿ∏• ∞°øÎ≥ÎµÂ ¿ÃæÓ¡÷¥¬ ∆„º«¿Œ∞≈ ∞∞¿∫µ• ≥ π´ ±ÊæÓº≠ ∞°Ω√º∫¿Ã ∂≥æÓ¡Æº≠ æ∆Ω¨øÓ∫Œ∫–

2. detach_freeø°º≠ head_list_ptr¿Ã null∞™¿ª ∞°¡ˆ∞Ì ¿÷¥¬ ∞ÊøÏ∏¶ ª˝∞¢ æ»«ÿ¡‡µµ µ«≥™?

3. realloc ∫Œ∫–¿∫ ∫–±‚∏¶ ≥™¥≤º≠ æ»¬ ø° ∫Û∞¯∞£ ¿÷¿∏∏È ≥÷¿∏∏È ¥ı ¡¡¥Ÿ¥¬∞≈ ∞∞¥Ÿ¥¬µ• ¿˙µµ ±∏«ˆ¿∫ æ»«ÿ∫√Ω¿¥œ¥Ÿ.

¥Ÿ∏•∫Œ∫–¿∫ ∞≈¿« ∫ÒΩ¡«ÿº≠ ¿ﬂ ¬ß∞≈ ∞∞∞Ì, ¿ﬂ µπæ∆∞£¥Ÿ∏È πÆ¡¶¥¬ æ¯¿ª∞≈ ∞∞Ω¿¥œ¥Ÿ.

/*