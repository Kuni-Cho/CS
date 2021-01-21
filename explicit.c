#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
#define WSIZE 4             // 1 워드 크기
#define DSIZE 8             // 2 워드 크기
#define CHUNKSIZE (1 << 12) //전체 단위 사이즈 2^12 (페이지 크기)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))       // | 연산자로, 크기와 할당 비트를 합쳐서 header, footer에 넣는다.
#define GET(p) (*(unsigned int *)(p))              //포인터의 값을 가져온다.
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // 해당 포인터에 값을 입력한다.
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp)-WSIZE) // 현재 포인터에서 1워드 크기 전으로 돌아가면 헤드
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) //footer에서 size를 가져온다.
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
//프롤로그 헤더 뒤 주소를 가리킨다.

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(24)) == (void *)-1) // 최초 최소 크기 지정
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
    if (extend_heap(CHUNKSIZE / DSIZE) == NULL) // 힙 확장 실패
        return -1;
    return 0; // 힙 확장 성공
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    // size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    size = words * DSIZE;

    printf("extend_heap_words : %d\n", words);
    printf("extend_heap_size : %d\n", size);

    // 신규 공간 생성이 실패
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // 신규 공간이 생성이 된 경우
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
    // 남은 공간이 최소 블록 크기보다 큰 경우, 남은 공간을 분할해 준다.
    if (tmp_result >= 2 * DSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);

        // 채워지고 남은 블럭을 할당되지 않은 상태로 초기화 시킨다.
        PUT(HDRP(bp), PACK(tmp_result, 0));
        PUT(FTRP(bp), PACK(tmp_result, 0));

        // 가용 상태가 되므로, 가용리스트에 이어준다.
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
    else // 앞 뒤 블록이 모두 미할당일 경우,
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
    PUT(HDRP(bp), PACK(size, 0)); // 할당 값 반환하기
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void free_delete(void *ptr)
{
    // 1. 가용 리스트의 맨 처음인 경우
    if (ptr == head_list_ptr)
    {
        PUT(PREV_FREEBLKP(GET(NEXT_FREEBLKP(ptr))), NULL);
        head_list_ptr = GET(NEXT_FREEBLKP(ptr));
    }
    else
    // 2. 가용 리스트의 중간인 경우
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