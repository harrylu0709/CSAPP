#include "cachelab.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
typedef struct cache_line
{
    int valid;
    int tag;
    int counter;
}Cache_line_t;
Cache_line_t** cache = NULL;

int isVerbose = 0;

int getSet(int address, int s, int b)
{
    address = address >> b;
    int mask = (1<<s) -1;
    return address & mask;
}

int getTag(int address, int s, int b)
{
    int len = b + s;
    return address>>len;
}

void initCache(int S, int e)
{
    int i,j;
    int E = e;
    cache = (Cache_line_t**)malloc(sizeof(Cache_line_t*)*S);
    for (i = 0; i < S; i++)
    {
        cache[i] = (Cache_line_t*)malloc(sizeof(Cache_line_t)*E);
    }
    for (i = 0; i < S; i++)
    {
        for (j = 0; j < E; j++)
        {
            cache[i][j].valid = 0;
            cache[i][j].tag = -1;
            cache[i][j].counter = -1;
        }
    }
}

void freeCache(int S)
{
    int i;
    for (i = 0; i < S; i++)
    {
        free(cache[i]);
    }
    free(cache);
}
/*
https://blog.csdn.net/m0_64097945/article/details/122781385
https://blog.csdn.net/Mculover666/article/details/106646339
gcc -o csim csim.c cachelab.c
*/

void printCache(int S, int E)
{
    for(int i = 0; i< S; i++)
    {
        for(int j= 0; j< E; j++)
        {
            printf("\ncache[%d][%d] v:%d t:%d c:%d",i, j, cache[i][j].valid, cache[i][j].tag, cache[i][j].counter);
        }
    }
    printf("\n--------\n");
}


void visitCache(int addr, int set, int tag, int* numHit, int* numMiss, int* numEvict, int S, int E)
{
#if 0
    int i;
    int empty_line = -1;
    int temp = cache[set][0].counter;
    int idx = 0;
    for(i= 0; i < E; i++)
    {
        if((cache[set][i].valid == 1))
        {
            if((cache[set][i].tag) == tag)
            {
                if(isVerbose) printf(" hit");
                (*numHit)++;
                cache[set][i].counter = 0;
                return;                
            }
            else
            {
                (cache[set][i].counter)++;
            }
            if(cache[set][i].counter >= temp)
            {
                temp = cache[set][i].counter;
                idx = i;
            }
        }
        else{
            empty_line = i;
        }
    }
    (*numMiss)++;
    
    
    if(empty_line != -1)
    {
        if(isVerbose) printf(" miss");
        cache[set][empty_line].valid = 1;
        cache[set][empty_line].tag = tag;
        cache[set][empty_line].counter = 0;
        return;
    }
    else
    {
        if(isVerbose) printf(" eviction");
        (*numEvict)++;
        cache[set][idx].valid = 1;
        cache[set][idx].tag = tag;
        cache[set][idx].counter = 0;
        return;
    }
#else
    int i;
    for(i= 0; i < E; i++)
    {
        if(cache[set][i].valid == 1)
        {
            (cache[set][i].counter)++;
            if((cache[set][i].tag) == tag)
            {
                if(isVerbose) printf(" hit");
                (*numHit)++;
                cache[set][i].counter=0;
                return;
            }
            
        }
        // if((cache[set][i].valid == 1) && (cache[set][i].tag) == tag)
        // {
        //     if(isVerbose) printf(" hit");
        //     (*numHit)++;
        //     cache[set][i].counter=0;
        //     return;
        // }
        // else if((cache[set][i].valid == 1) && (cache[set][i].tag) != tag)
        // {
        //     (cache[set][i].counter)++;
        // }
    }

    for(i= 0; i < E; i++)
    {
        if((cache[set][i].valid) == 0)
        { 
            if(isVerbose) printf(" miss");
            cache[set][i].valid = 1;
            cache[set][i].tag = tag;
            cache[set][i].counter= 0;
            (*numMiss)++;
            return;
        }
    }
    (*numMiss)++;
    (*numEvict)++;

    if(isVerbose) printf(" miss eviction");
    int temp = cache[set][0].counter;
    int idx = 0;
    for(i= 0; i < E; i++)
    {
        if(cache[set][i].counter >= temp)
        {
            temp = cache[set][i].counter;
            idx = i;
        }
    }
    cache[set][idx].tag = tag;
    cache[set][idx].counter = 0;

    return;
#endif

}

int main(int argc, char* argv[])
{
#if 1
    int numHit = 0;
    int numMiss = 0;
    int numEvict = 0;
    int opt ,s , E, b;
    char filename[50];
    // char *optarg;
    /* looping over arguments */
    while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1){
    /* determine which argument it’s processing */
        switch(opt) {
            case 'v':
                isVerbose=1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                strcpy(filename, optarg);
                break; 
            default:
                printf("wrong argument\n");
                break;
        }
    } 
    int S = 1<<s;
    initCache(S, E);
    
    FILE* pFile;	//pointer	to	FILE	object	
    pFile = fopen(filename,"r");	//open	file	for	reading	
    char identifier;	
    int address;	
    int	size;	
    // //	Reading	lines	like	"	M	20,1"	or	"L	19,3"	[space]operation address,size
    while(fscanf(pFile, " %c %x,%d\n",	&identifier, &address, &size) != EOF)	
    {	
        if(isVerbose && identifier != 'I')
        {
            printf("\n%c %x,%d",identifier, address, size);
        } 
        if(identifier == 'I') continue;
        int set = getSet(address, s ,b);
        int tag = getTag(address, s ,b);

        // printf("%c addr %x set %x  tag %x\n",identifier,address, set, tag);
        if(identifier == 'L'){
            visitCache(address, set, tag, &numHit, &numMiss, &numEvict, S, E);
            // printCache(s,E);
        }
        if(identifier == 'S'){
            visitCache(address, set, tag, &numHit, &numMiss, &numEvict, S, E);
            // printCache(s,E);
        }    
        if(identifier == 'M'){
            visitCache(address, set, tag, &numHit, &numMiss, &numEvict, S, E);
            // printCache(s,E);
            visitCache(address, set, tag, &numHit, &numMiss, &numEvict, S, E);
            // printCache(s,E);
        }
        if(isVerbose) printf("\n");
        // printf("hits:%d misses:%d evictions:%d\n",numHit, numMiss, numEvict);
    }	
    fclose(pFile);	//remember	to	close	file	when	done	
    printSummary(numHit, numMiss, numEvict);
    free(cache);
#endif
    return 0;
}
