#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define INITSIZE 10 

typedef unsigned int uint;
typedef int Etype;

typedef struct sqlist
{
  Etype* elem;
  uint length;
  uint m_size;
} SQLIST;

void initSqlist(SQLIST* inlist) {
  inlist->elem = (Etype*)malloc(sizeof(Etype)*(INITSIZE + 2) + 1); 
  inlist->length = 0;
  inlist->m_size = INITSIZE;
}

void initSqlInsert(SQLIST* inlist, Etype ielem, uint position) {
  int i;
  Etype* newbase;
  if (position > inlist->length + 1 || position < 1) {
    printf("line table must continuous or position must > 0\n");
    exit(1);
  }
  
  // if memory small give more memory
  if(inlist->length + 1 >= inlist->m_size) {
    if (! (newbase = (Etype*)realloc(inlist->elem, (inlist->length + INITSIZE + 2)* sizeof(Etype) + 1))) {
      printf("mem alloc failed\n");
      exit(2);
    }
    inlist->elem = newbase;
    inlist->m_size = inlist->m_size + INITSIZE;
  }

  for (i = 0; i < (inlist->length - position + 2); i++) {
    *(inlist->elem + inlist->length + 1 - i) = *(inlist->elem + inlist->length - i);
  }

  *(inlist->elem + inlist->length + 1 - i) = ielem;
  inlist->length = inlist->length + 1;

}

void view(SQLIST* inlist) {
  int i;
  if (inlist->length == 0) {
    printf("no data\n");
    exit(3);
  }

  for (i = 0; i < inlist->length; i++) {
    printf("node:%d values is:%d data length:%d max_size:%d \n", i, *(inlist->elem + i), inlist->length, inlist->m_size);
  }
}

int main(void) {
  SQLIST a;
  initSqlist(&a);
  initSqlInsert(&a, 5, 1);
  view(&a);
}
