#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

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

/*basic constants and macros / macro :프로그램에서 사용되는 상수값을 정의할 때*/
#define WSIZE  4   /*word and header/footer size(byte)*/
#define DSIZE  8    /*double worad size(bytes)*/ 
#define CHUNKSIZE  (1<<12) /* 2의 12거듭제곱(4096) extned heap by this amount(bytes)*/

#define MAX(x,y) ((x) > (y) ? (x):(y)) //macro MAX(x,y) 정의 : 삼항연산자, 조건이 참이면 첫번째 값(x)반환, 거짓이면 두번째 값(y)반환

/*pack a size and allocated bit into a word : 메모리 블록의 헤더에서 사용되는 정보를 한 단어(word)에 압축(pack)하여 저장하기*/
#define PACK(size,alloc) ((size) | (alloc)) // 비트 or 연산자로 결합하여 하나의 값을 반환 size: 메모리블록의 크기, alloc:해당 블록의 할당 여부(0 or 1)

/* Read and write a word at address p , 주소 p에서 word를 읽거나 쓰기(해당 주소에 접근하는 메모리 주소 버스를 통해 데이터 버스에 데이터를 전송하거나, 데이터 버스에서 데이터를 읽어와 해당 주소의 메모리에 저장하는 작업이 필)*/
#define GET(p) (*(unsigned int *)(p)) //p가 가리키는 메모리 위치에 저장된 데이터 읽어옴
#define PUT(p, val) (*(unsigned int *)(p) = (val))//p가 가리키는 메모리 위치에 데이터 val 저장

/*Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) //사이즈 정도는 최하위 3비트를 제외한 나머지 비트에 저장, 이를 위해 & ~0x7비트 연산을 사용해 최하위 3비트를 모두 0으로 만듦=>할당 블록의 크기 나타냄
#define GET_ALLOC(p) (GET(p) & 0x1) //할당 여부 정보는 최하위 1비트에 저장, 이를 위해 & 0x1 비트 연산을 사용해 최하위 1비트를 추출 

/*Given block ptr pb(블록 포인터 pb, 해당 블록의 시작 주소), compute address of its header and footer*/
#define HDRP(bp) ((char *)(bp) - WSIZE) //bp point에서 -4byte 해서 header 주소계산(header : 해당 블록 사이즈와 할당 여부 등의 정보 저장)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // bp + 블록 사이즈 - DSIZE => footer 포인터 알 수 있음

/*Given block ptr pb, compute address of next and previous blocks*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //GET_SIZE(((char *)(bp) - WSIZE)) ->header주소 : 메모리 블럭 사이즈 + bp => next_pb 알 수 있음
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //GET_SIZE(((char *)(bp) - DSIZE)) = (이전 footer 주소)해당 메모리 블럭 크기, bp- 구한 메모리 블럭크기=> 이전 블럭 bp

/*Explicit 할당해제된 연결리스트 이전, 다음 주소 값 가져오기*/
#define SUCC_freep(bp) (*(void**)(bp+WSIZE))
#define PRED_freep(bp) (*(void**)(bp))

static char *heap_listp;
static char *free_listp;

int mm_init(void)
{
    /*create the initial empty heap*/
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /*Alignement padding, heap_listp 가 가르키는 위치에 0 삽입*/ 
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); /*PUT(주소, 값)Prologue header*/
    PUT(heap_listp + (2*WSIZE), NULL); //PRED(predecessor)
    PUT(heap_listp + (3*WSIZE), NULL); //SUCC(successor)
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1)); /*Prologue footer*/
    PUT(heap_listp + (5*WSIZE), PACK(0,1)); /*Epilogue header*/
    
    free_listp = heap_listp +(2*WSIZE); 
   // printf("free_listp: %x\n",free_listp);

    //root = heap_listp;
    /*Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) 
        return -1;
    return 0;
}

//새 free블록을 free list의 처음에 추가=>pred는 항상 null(맨앞이니깐)
static void putFreeBlock(void *bp){
    PRED_freep(bp) = NULL;
    SUCC_freep(bp) = free_listp;
    PRED_freep(free_listp) = bp;
    free_listp = bp;
}

static void removeFreeBlock(void *bp){
    //주소 받으면, succ과 pred을 null 하고, succ과 pred 를 서로 연결한 다음 맨앞에 블록으로 bp free_lisp 만들기,,,

    SUCC_freep(PRED_freep(bp)) = SUCC_freep(bp);
    PRED_freep(SUCC_freep(bp)) = PRED_freep(bp);
    // char *next_block = SUCC_freep(bp);
    // char *pre_block = PRED_freep(bp);

    // if(bp== free_listp){
    //     free_listp = SUCC_freep(bp);
    //     PRED_freep(SUCC_freep(bp)) = NULL; 
    // }
    // else{ //중간이면
    //     printf("확인\n");
    //     printf("bp: %x\n",bp);
    //     printf("pre_block: %x\n",pre_block);
    //     printf("next_block: %x\n",next_block);
    //     SUCC_freep(PRED_freep(bp)) = SUCC_freep(bp);
    //     PRED_freep(SUCC_freep(bp)) = PRED_freep(bp);
    // }
}

//블록 연결하기
static void *coalesce(void *bp){
    //할당 해제할 메모리 주소 받음 -> 해제할 메모리 주소의 앞뒤 블록을 확인
    size_t prev_alloc;
    size_t next_alloc;

    prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));        
    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // printf("prev_alloc: %d\n",prev_alloc);
    // printf("next_alloc: %d\n",next_alloc);
    size_t size = GET_SIZE(HDRP(bp)); 

    // if(prev_alloc && next_alloc){   
    // /*case1 :앞뒤블록이 모두 할당된 상태이면 합병할 블록 없으므로 현재블록을 연결리스트 첫번째에 연결*/
    //     putFreeBlock(bp);         
    //     return bp;
    // }
    /*case2:앞블록 할당, 뒤 블록 할당 X => 현재블록과 뒤블록 합병*/
    if(!prev_alloc && !next_alloc){
        putFreeBlock(bp);
        return bp;
    }
    else if(prev_alloc && !next_alloc){       
        //1. 뒤블록 free_list에 제거
        char *next_block_bp = NEXT_BLKP(bp);
        removeFreeBlock(NEXT_BLKP(bp)); //뒤 블록 연결해제
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0)); 
    }
    /*case3: 앞블록 할당X, 뒤 블록 할당 => 현재블록과 앞블록 합병*/
    else if (!prev_alloc && next_alloc)   
    {
        char *pred_block_bp = PREV_BLKP(bp); //앞블록 주소 가져오기
        removeFreeBlock(PREV_BLKP(bp));//앞블록 freeList 연결 끊기
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); //이전 블록사이즈를 더함
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(bp), PACK(size,0));
        bp = PREV_BLKP(bp);
        putFreeBlock(bp);
    }
    else if(!prev_alloc && !next_alloc){   /*case4*/
        char *pred_block_bp = PREV_BLKP(bp); //앞블록 주소 가져오기
        char *next_block_bp = NEXT_BLKP(bp); //뒤블록 주소 가져오기
        removeFreeBlock(PREV_BLKP(bp));
        removeFreeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
        
    }   
    putFreeBlock(bp);
    return bp;
}
 
/*
1. 힙이 초기화 2. mm_malloc이 적당한 fit를 찾지 못했을때
*/
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    /*Allocate an even number of words to maintain alignment*/
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; //홀수이면 (words + 1) * WSIZE, 짝수이면 뒤에꺼
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /*Initialize free block header/footer and the epilogue header*/
    PUT(HDRP(bp), PACK(size,0)); /*Free block header*/
    PUT(FTRP(bp), PACK(size,0)); /*Free block footer*/
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); /*New epilogue header*/

    /*Coalesce if the previous block was free : 이전 블록이 사용가능 시 병합*/
    return coalesce(bp);   
}


//인자로 받은 크기(size)에 맞는 블록을 찾아 할당하거나, 적절한 크기의 새로운 블록을 할당하고 반환
void *mm_malloc(size_t size)
{
    size_t asize; /*Adjusted block size*/
    size_t extendsize; /* Amount to extend heap if no fit*/
    char *bp;
    
    /*Ignore spurious requests*/
    if(size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if(size <= DSIZE) //DSIZE보다 작다면
        asize = 2*DSIZE; //최소 크기인 2*DSIZE(헤더, 푸터, 최소 1워드 크기에 데이터)로 할당
    else
        //asize = DSIZE의 배수
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //DSIZE: 헤더와푸터: 8bytes
    
    /* Search the free list for a fit */
    if((bp = find_fit(asize)) != NULL){
        place(bp,asize); // 실제로 메모리를 할당받고, 값을 업데이트
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) //extendsize/WSIZE word(4byte)로 변환
        return NULL;
    place(bp,asize);
    return bp;
}

//힙을 탐색 해 요구하는 메모리 공간보다 큰 가용 블록의 주소를 반환
static void *find_fit(size_t asize){
    void *bp;

    //SUCC_freep(bp) != NULL;
    for(bp = free_listp; !GET_ALLOC(HDRP(bp)); bp = SUCC_freep(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            //printf("asize : %d\n\n",asize);
            return bp;
        }
    }
    return NULL; /*no fit*/
}

//요구 메모리를 할당할 수 있는 가용블록을 할당함 , 분할 가능 시 분할
static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp)); //현재 블록의 크기
    //void *tmp_succ = SUCC_freep(bp); //현재 블록의 연결된 다음 주소값
    //void *tmp_pred = PRED_freep(bp); //현재 블록의 연결된 이전 주소값
    printf("place: %u\n",bp);
    removeFreeBlock(bp);
    //csize - asize 
    if((csize - asize) >= (2*DSIZE)){ //분할 후 남은 블록의 크기가 최소블록 크기(16bytes) 일 시
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp),PACK(csize - asize,0)); //남은 공간 가용 상태 변경
        PUT(FTRP(bp),PACK(csize - asize,0));

        putFreeBlock(bp);
    }
    else{

        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));

        // if(tmp_pred == free_listp){ //free_list에 첫번째니깐 
        //     removeFreeBlock(bp);
        //     PUT(HDRP(bp),PACK(csize,1));
        //     PUT(FTRP(bp),PACK(csize,1));
        //     // free_listp = tmp_succ;
        //     // SUCC_freep(bp) = NULL;

        // }
        // else{ // free_list의 중간에 위치한 블록을 할당함
        //     removeFreeBlock(bp);
        //     PUT(HDRP(bp),PACK(csize,1));
        //     PUT(FTRP(bp),PACK(csize,1));

            // SUCC_freep(tmp_pred) = tmp_succ;
            // PRED_freep(tmp_succ) = tmp_pred;
        // }
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) // 해당 주소의 블록을 반환
{
    size_t size = GET_SIZE(HDRP(bp)); //bp의 메모리 크기

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
//mm_malloc 함수에 의해 이전에 할당된 메모리 블록의 크기를 조정하는 사용되는 함수
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    //재할당되는 메모리 블록의 크기를 결정
    //copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE); //이전 메모리 블록의 크기가 이전 ptr이 가르키는 메모리 블록 바로 앞에 있다고 가정
    copySize = GET_SIZE(HDRP(oldptr)); //원래 블록의 사이즈

    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize); //string.h 내장 함수: 메모리블록을 한위치에서 다른 위치로 복사하는데 사용
    mm_free(oldptr);
    return newptr;
}