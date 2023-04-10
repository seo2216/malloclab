#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Team2",
    /* First member's full name */
    "Sangun Lee",
    /* First member's email address */
    "sangunlee6@gmail.com",
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

// 초기 가용 리스트 조작을 위한 기본 상수 및 매크로 정의
#define WSIZE  4    
#define DSIZE  8    
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x)>(y)? (x):(y))  

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p)) //p가 가리키는 메모리 위치에 저장된 데이터 읽어옴
#define PUT(p, val) (*(unsigned int *)(p) = (val))//p가 가리키는 메모리 위치에 데이터 val 저장

// #define PUT(p, add) (*(unsigned int *)(p) = (unsigned int *)(add))

#define GET_SIZE(p)    (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)    ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)    ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// #define PUT_PREV_ADDR(bp, address) (*(unsigned int *)(bp) = (address)) /* 현재 블록 포인터에 PREV 주소를 설정함 */
// #define PUT_NEXT_ADDR(bp, address) (*(unsigned int *)((char *)bp + WSIZE) = (address))
// #define PREV_FREE(bp) (*(unsigned int *)(bp))          /* 이전 프리 블록의 주소 */
// #define NEXT_FREE(bp) (*(unsigned int *)(bp + WSIZE))  /* 다음 프리 블록의 주소 */

static char *root;
static char *heap_listp;

int mm_init(void)
{
  if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *) -1)
    {return -1;}
  PUT(heap_listp, 0);
  PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE,1));
  PUT(heap_listp + (2*WSIZE), NULL);
  PUT(heap_listp + (3*WSIZE), NULL);
  PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE,1));
  PUT(heap_listp + (5*WSIZE), PACK(0,1));

  root = heap_listp+(2*WSIZE);
  heap_listp += (2*WSIZE);

  if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
      return -1;}
  
//   PUT(heap_listp, (unsigned int*)(heap_listp + (4*WSIZE)) );
//   PUT(heap_listp + 4*WSIZE, NULL);
//   PUT(heap_listp + 5*WSIZE, (unsigned int*)heap_listp);

//   root = NEXT_BLKP(heap_listp);

  return 0;
}

// static void removeFreeBlock(void *bp){
//     //현재 주소 기준으로 이전블록과 다음블록 가져오기
//     //unsigned int *pre_freeBlock_adress = (*(unsigned int*)(bp));
//     //unsigned int *next_freeBlock_adress = (*(unsigned int*)(bp+WSIZE));
//     // printf("bp %x\n", bp);
//     // printf("h %x\n", heap_listp);
//     if(bp == root){ //현재 bp가 freeList에 맨 앞을 가리킨 주소와 같다면
//         root = NEXT_FREE(bp); //root는 다음 freeBlock을 가리킴
//         PUT_PREV_ADDR(NEXT_FREE(bp), NULL); //nextblock에 이전블록은 null임
//     }
//     else{ //중간이면
//         PUT_NEXT_ADDR(PREV_FREE(bp), NEXT_FREE(bp)); //이전블록에 next는 현재블록의 nextblock임
//         if(NEXT_FREE(bp) != NULL){
//             PUT_PREV_ADDR(NEXT_FREE(bp), PREV_FREE(bp)); //다음블록에 이전블록은 현재블록의 preblock
//         }

//     }
// }

static void removeFreeBlock(void *bp){
    //현재 주소 기준으로 이전블록과 다음블록 가져오기
    char *pre_freeBlock_address = GET(bp);
    char *next_freeBlock_address = GET((char *)bp+WSIZE);

    if(bp == root){ //현재 bp가 freeList에 맨 앞을 가리킨 주소와 같다면
        root = next_freeBlock_address; //root는 다음 freeBlock을 가리킴
        PUT(next_freeBlock_address, NULL); //nextblock에 이전블록은 null임
    }
    else{ //중간이면
        PUT(pre_freeBlock_address + WSIZE, next_freeBlock_address); //이전블록에 next는 현재블록의 nextblock임
        if(GET((char*)bp+WSIZE) != NULL) {
            PUT(next_freeBlock_address, pre_freeBlock_address); //다음블록에 이전블록은 현재블록의 preblock
        }
    }
}

static void insertFreeBlock(void *bp){
//1. 현재 주소의 next가 root를 가리킴
//2. root의 이전을 bp 가리킴
//3. ROOT =BP   

    // PUT_NEXT_ADDR(bp, root);
    // //PUT((char*)bp + WSIZE, root);
    // if (bp != NULL){
    //     PUT_PREV_ADDR(root, bp);
    //     //PUT(root, bp);   
    // }
    // root = bp;


    PUT((char*)bp + WSIZE, root);
    PUT(root, bp);
    root = bp;

    // PUT((char*)bp + WSIZE, root);
    // PUT(bp, NULL);
    // root = bp;

    
}

static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));


  if (prev_alloc && next_alloc){
    insertFreeBlock(bp);
    return bp;    
  }

  else if (prev_alloc && !next_alloc){ //뒤에 할당 안됨! 뒤에 연결 끊기
    removeFreeBlock(NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    
    insertFreeBlock(bp);
    return bp;     
  }

  else if (!prev_alloc && next_alloc){ //앞에 할당 안됨! 앞에 연결 끊기
    removeFreeBlock(PREV_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    bp = PREV_BLKP(bp);    

    // unsigned int *temp = root;
    // root = (unsigned int*)bp;
    // PUT(bp, NULL);
    // PUT(bp + WSIZE, temp);
    insertFreeBlock(bp);
    return bp;     
  }

  else {
    removeFreeBlock(NEXT_BLKP(bp));
    removeFreeBlock(PREV_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));    
    bp = PREV_BLKP(bp);    
    
    // unsigned int *temp = root;
    // root = bp;
    // PUT(bp, NULL);
    // PUT(bp + WSIZE, temp);
    insertFreeBlock(bp);
    return bp;     
  }
}

static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;


  size = (words %2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;

  PUT(HDRP(bp), PACK(size,0));
  PUT(FTRP(bp), PACK(size,0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

  return coalesce(bp);
}

void mm_free(void *bp)
{
  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

void *mm_malloc(size_t size)
{
  size_t asize;
  size_t extendsize;
  char *bp;

  if (size ==0)
    return NULL;
  
  if (size <= DSIZE)
    asize = 3*DSIZE;
  else 
    asize = DSIZE + (DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE)); // 앞에 DSIZE 는 prev_bp + next_bp 

  if ((bp = find_fit(asize)) != NULL){
    
    place(bp, asize);
    return bp;
  }

  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;
  
  place(bp, asize);
  return bp;
}

static void *find_fit(size_t asize)
{
  void *bp;
  // for(bp = root; GET(bp+WSIZE) != NULL; bp=GET(bp+WSIZE)){
  for(bp = root;  GET((char*)bp+WSIZE) != NULL; bp=GET((char*)bp+WSIZE)){
    // printf("find_fit 안에 bp: %x\n", bp);
    // printf("HDRP(bp): %x\n",HDRP(bp));
    // printf(" GET(bp+WSIZE): %x\n",  GET(bp+WSIZE));
    // printf("GET_SIZE(HDRP(bp)): %u\n",GET_SIZE(HDRP(bp)));
    // printf("asize: %u\n",asize);
    if (asize <= GET_SIZE(HDRP(bp))){
        return bp;
    }
  }
  return NULL;
}


static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));
//   char* temp_prev_bp = GET(bp);
//   char* temp_next_bp = GET((char *)bp + WSIZE);


  if ((csize-asize) >= (3*DSIZE)) {
    removeFreeBlock(bp);
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    bp = NEXT_BLKP(bp);

    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));

    insertFreeBlock(bp);
  }
  else{
    removeFreeBlock(bp);
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }

//     PUT(bp, NULL);
//     PUT(bp + WSIZE, NULL);

//     bp = (unsigned int*)NEXT_BLKP(bp);
//     PUT(HDRP(bp), PACK(csize - asize, 0));
//     PUT(FTRP(bp), PACK(csize - asize, 0));
//     PUT(bp, temp_prev_bp);
//     PUT(bp + WSIZE, temp_next_bp);

//     if (temp_prev_bp != NULL) {
//       PUT(temp_prev_bp + WSIZE, (unsigned int*)bp);
//     }
//     else {
//       root = (unsigned int*)bp;
//     }

//     if (temp_next_bp != NULL) {
//       PUT(temp_next_bp, (unsigned int*)bp);
//     }
//     else {
//       root = (unsigned int*)bp;
//     }
//   }
//   else {
//     PUT(HDRP(bp), PACK(csize, 1));
//     PUT(FTRP(bp), PACK(csize, 1));
//     PUT(bp, NULL);
//     PUT(bp + WSIZE, NULL);

//     if (temp_prev_bp != NULL) {
//       PUT(temp_prev_bp + WSIZE, temp_next_bp);
//     }
//     else {
//       root = temp_next_bp;
//     }

//     if (temp_next_bp != NULL) {
//       PUT(temp_next_bp, (unsigned int*)temp_prev_bp);
//     }
//   }
}


void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}