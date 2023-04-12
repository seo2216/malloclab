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
#define SUCC_freep(bp) (*(char **)(bp+WSIZE))
#define PRED_freep(bp) (*(char **)(bp))

#define NUM_LIST 12 // 일단 정의..

static char *heap_listp;
static char *free_listp;

static char *root_array[NUM_LIST];

int mm_init(void)
{
    /*create the initial empty heap*/
   printf("mm_init() \n");

    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /*Alignement padding, heap_listp 가 가르키는 위치에 0 삽입*/ 
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); /*PUT(주소, 값)Prologue header*/
    PUT(heap_listp + (2*WSIZE), PACK(2*DSIZE, 1)); /*Prologue footer*/
    PUT(heap_listp + (3*WSIZE), PACK(0,1)); /*Epilogue header*/
    
    heap_listp += (2*WSIZE);

    for(int i = 0; i<NUM_LIST; i++){
        root_array[i] = NULL;
    }
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) 
        return -1;
    return 0;
}

static int find_index(size_t size){
   printf("find_index() \n");


//    for (int i = 0; i<NUM_LIST; i++){
//         if(size < 1<<i){
//             return i;
//         }
//         return NULL;
//    }

    if(size >= (1<<4) && size <(1<<5)){
        return 4;
    } 
    else if(size >= (1<<5) && size <(1<<6)){
        return 5;
    }
    else if(size >= (1<<6) && size <(1<<7)){
        return 6;
    }
    else if(size >= (1<<7) && size <(1<<8)){
        return 7;
    }
    else if(size >= (1<<8) && size <(1<<9)){
        return 8;
    }
    else if(size >= (1<<9) && size <(1<<10)){
        return 9;
    }
    else if(size >= (1<<10) && size <(1<<11)){
        return 10;
    }
    else if(size >= (1<<11) && size <(1<<12)){
        return 11;
    } 
}
static int find_size(size_t size){
    printf("find_size() \n");
    for(int i=4; i<NUM_LIST;i++){ //NUM_LIST+1 ???
        if(size <= (1<<i)){
            return (1<<i);
        }
    }
    return NULL;
}

static void putFreeBlock(void *bp){

    printf("putFreeBlock() \n");

    size_t bp_size = GET_SIZE(HDRP(bp)); //1. bp_size를 가져옴

    int root_array_index = find_index(bp_size); //해당 크기에 해당하는 root_array index가져옴
    char* root_bp = root_array[root_array_index];//bp_size에 해당하는 root_array에 담겨있는 값을 가져옴

    if(root_bp == NULL){
        //root가 null => 해당 사이즈에 맞는 free block이 아무것도 없다!
        PRED_freep(bp) = root_bp; //해당 bp가 가리키는 block은 맨앞이므로 null가르킴 
        SUCC_freep(bp) = NULL;    
        //PUT(&root_array[root_array_index],(char*)(bp)); //root는 bp 주소를 가리킴
        root_array[root_array_index] = bp;
        //printf("root_array[root_array_index] %x\n",(unsigned int*)root_array[root_array_index]); //char 32
    }
    else{
        PRED_freep(bp) = &root_array[root_array_index]; //맨 앞 블럭에 이전은 배열의 주소를 가리킴
        SUCC_freep(bp) = root_bp; //root_bp가 가리키고 있던 주소
        PRED_freep(root_bp) = bp; //root_bp가 가리키고 있던 주소의 이전 블록은 bp
        //PUT(root_bp,(char*)(bp)); //root는 bp 주소를 가리킴
        root_array[root_array_index] = bp;
    }
}

static void removeFreeBlock(void *bp){

    printf("removeFreeBlock() \n");

    size_t bp_size = GET_SIZE(HDRP(bp)); //1. bp_size를 가져옴
    int root_array_index = find_index(bp_size); //해당 크기에 해당하는 root_array index가져옴

    if(bp == root_array[root_array_index]){ //freeblock 맨 앞이면
        root_array[root_array_index] = SUCC_freep(bp);
        if(SUCC_freep(bp) != NULL){
            PRED_freep(SUCC_freep(bp)) = &root_array[root_array_index]; 
        }
    }
    else{ //중간이면
        if(SUCC_freep(bp) == NULL){ //맨 끝이면
            SUCC_freep(PRED_freep(bp)) = NULL;
        }
        SUCC_freep(PRED_freep(bp)) = SUCC_freep(bp); //중간일때

        if(SUCC_freep(bp) != NULL){
            PRED_freep(SUCC_freep(bp)) = PRED_freep(bp);
        }
    }
}

//블록 연결하기
static void *coalesce(void *bp){

printf("coalesce() \n");
    size_t prev_alloc;
    size_t next_alloc;

    prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));        
    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp)); 


    //next_bp = bp+size; 다음 블록 주소
    // 현재 bp와 size 를 '&' 비트 연산한 결과의 맨 첫자리 가 1이면 난 오른쪽, 0이면 왼쪽

    //while 돌면서 계속 확인하디....
    while(1){
        if(!(((char*)bp-(heap_listp+4)) & size)){ //AND연산 결과가 0 : 나는 왼쪽임
            // buddy인 경우: 옆 블록이 free이고, size가 같다면 합쳐줌
            if(GET_ALLOC(HDRP((long*)bp+size)) == 0 && GET_SIZE(HDRP((long*)bp+size)) == size){
                removeFreeBlock((long*)bp+size);
                PUT(HDRP(bp), PACK(2*size,0));
                PUT(FTRP(bp), PACK(2*size,0));
                putFreeBlock(bp);
            }
            else{ //buddy가 아닌 경우
                putFreeBlock(bp);
                break;
            }
        }
        else{ //나는 오른쪽
            //내 왼쪽이 buddy라면
            printf("HDRP((long*)bp-size): %x\n",HDRP((long*)bp-size));
            printf("HDRP((long*)bp-size): %x\n",HDRP(bp));
            printf("GET_SIZE(HDRP((long*)bp-size)): %x\n",GET_SIZE(HDRP((long*)bp-size)));
            // printf("GET_ALLOC(HDRP((long*)bp-size)): %d\n",GET_ALLOC(HDRP((long*)bp-size)));
            // printf("GET_SIZE(HDRP((long*)bp-size)): %d\n",GET_SIZE(HDRP((long*)bp-size)));
            if(GET_ALLOC(HDRP((long*)bp-size)) == 0 && GET_SIZE(HDRP((long*)bp-size)) == size){
                removeFreeBlock((long*)bp-size);
                PUT(HDRP((long*)bp-size), PACK(2*size,0));
                PUT(FTRP(bp), PACK(2*size,0));       
                putFreeBlock((long*)bp-size);
            }
            else{
                putFreeBlock(bp);
                break;
            }

        }
    }
    return bp;
}
 
static void *extend_heap(size_t words){

    printf("extend_heap() \n");

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
    printf("mm_malloc() \n");

    size_t asize; /*Adjusted block size*/
    size_t extendsize; /* Amount to extend heap if no fit*/
    char *bp;
    
    /*Ignore spurious requests*/
    if(size == 0)
        return NULL;

    //find_2^n size 함수 호출해야함!!!!!!
    asize = find_size(size); //2^n size

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

    printf("find_fit()\n");

    void *bp;
    void *root;
    char *root_bp;

    int root_array_index =  find_index(asize);

    for(int i = root_array_index; i < NUM_LIST; i++){
        if(root_array[i] == NULL){
            continue;
        }
        for(bp=root_array[i]; bp != NULL; bp=SUCC_freep(bp)){
            if(asize <=GET_SIZE(HDRP(bp))){
                return bp;
            }
        }                   
    }
    return NULL; /*no fit*/
}

//요구 메모리를 할당할 수 있는 가용블록을 할당함 , 분할 가능 시 분할
static void place(void *bp, size_t asize){
   printf("place()\n");

    size_t csize = GET_SIZE(HDRP(bp)); //현재 블록의 크기
    
    removeFreeBlock(bp); // 일단 잘라

    while(1){
        if(csize == asize){
            //할당
            PUT(HDRP(bp),PACK(asize,1));
            PUT(FTRP(bp),PACK(asize,1));
            break;
        }
        csize /= 2; //반으로 나눔
        putFreeBlock((long*)bp+csize); //csize 타입은 long
    }
}

void mm_free(void *bp) // 해당 주소의 블록을 반환
{
    printf("mm_free()\n");
    size_t size = GET_SIZE(HDRP(bp)); //bp의 메모리 크기

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    printf("mm_realloc()\n");
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    copySize = GET_SIZE(HDRP(oldptr)); //원래 블록의 사이즈

    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize); //string.h 내장 함수: 메모리블록을 한위치에서 다른 위치로 복사하는데 사용
    mm_free(oldptr);
    return newptr;
}