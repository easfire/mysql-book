#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define INITSIZE 10

typedef unsigned int uint;
typedef int Etype;

typedef struct sqlist
{
        Etype* elem; //pointer of sqlist base address
        uint length; //current length of elem
        uint m_size; //
} SQLIST;



void initsqlist(SQLIST* inlist)
{
        inlist->elem = (Etype* )malloc(sizeof(Etype)*(INITSIZE+2) + 1);
        inlist->length = 0;
        inlist->m_size = INITSIZE; //maxsize
}


void initsqlinsert(SQLIST* inlist,Etype ielem,uint postion) //insert elem before postion
{
        int i;
        Etype* newbase;
        if(postion > inlist->length + 1 || postion < 1)
        {
                printf("line table must continuous or postion must>0!\n");
                exit(1);
        }

        if(inlist->length + 1 >= inlist->m_size ) //if memory small will give more memory
        {
                if(!(newbase =(Etype* )realloc(inlist->elem,(inlist->length+INITSIZE+2)* sizeof(Etype)+1)))
                {
                        printf("mem alloc failed!\n");
                        exit(2);
                }
                inlist->elem = newbase;
                inlist->m_size = inlist->m_size+INITSIZE;
        }

        for(i=0;i<(inlist->length-postion+2);i++) //every elem +1
        {
                *(inlist->elem+inlist->length+1-i) = * (inlist->elem+inlist->length-i);
        }
        *(inlist->elem+inlist->length+1-i) = ielem; //give value
        inlist->length =inlist->length+1;
}

void delsqldel(SQLIST* inlist,uint postion) //delete elem of postion every elem -1
{
        int i;
        if((postion > inlist->length) ||(postion <1))
        {
                printf("give postion is must large than 1 and less than current length\n");
        }

        for(i=0;i<(inlist->length-postion) ;i++)
        {
                *(inlist->elem+postion+i) = *(inlist->elem+postion+i+1);
        }
        inlist->length =inlist->length-1;
}

void view(SQLIST* inlist)
{
        int i;
        if(inlist->length ==0 )
        {
                printf("init data arrary! no data found!\n");
                exit(3);
        }
        for(i=0;i<inlist->length;i++)
        {
                printf("node:%d values is:%d data length:%d max_size:%d \n",i,*(inlist->elem+i),inlist->length,inlist->m_size);
        }
}


int main(void)
{
    SQLIST a;
        initsqlist(&a);
        printf("insert two values\n");
        initsqlinsert(&a,5,1);
        initsqlinsert(&a,10,1);
        view(&a);

        printf("delete one values\n");
        delsqldel(&a,2);

        view(&a);

        printf("insert more than 10 values\n");
        initsqlinsert(&a,10,1);
        initsqlinsert(&a,20,1);
        initsqlinsert(&a,30,1);
        initsqlinsert(&a,40,1);
        initsqlinsert(&a,50,1);
        initsqlinsert(&a,10,1);
        initsqlinsert(&a,10,1);
        initsqlinsert(&a,10,1);
        initsqlinsert(&a,10,1);
        initsqlinsert(&a,10,1);
        initsqlinsert(&a,10,1);
        view(&a);


        return 0;
}