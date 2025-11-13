/* testhash.c: tests the hash table functions */
#include <stdio.h>
#include <stdlib.h>
#include "pf.h"
#include "pftypes.h"

int main()
{
    int i; long j; PFbpage *node;

    PFhashInit();
    /* insert a few entries */
    for (i=1; i < 11; i++)
        for (j=1; j < 11; j ++){
            node = (PFbpage*)malloc(sizeof(PFbpage));
            if (!node) { fprintf(stderr, "oom\n"); return 1; }
            if (PFhashInsert(i,(int)j,node) != PFE_OK){
                printf("PFhashInsert failed\n");
                return 1;
            }
        }

    /* Now, find all the entries */
    for (i=1; i < 11; i++)
        for (j=1; j < 11; j++){
            PFbpage *k = PFhashFind(i,(int)j);
            if (k == NULL){
                printf("PFfind failed at %d %ld\n",i,j);
                return 1;
            } else {
                printf("found \n");
            }
        }

    /* Now, delete them in reverse */
    for (j =10; j > 0; j--)
        for (i=10; i > 0; i--)
            if (PFhashDelete(i,(int)j) != PFE_OK){
                printf("PFhashDelete failed at %d %ld",i,j);
                return 1;
            }

    /* print the hash table out */
    PFhashPrint();
    return 0;
}
