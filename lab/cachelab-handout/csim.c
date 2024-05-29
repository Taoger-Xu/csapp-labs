#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include "cachelab.h"

// 全局变量
int s, E, b, verboseFlag = 0; // 接受命令行参数
char *tracefile;
int hits = 0, misses = 0, evictions; // 全局统计参数

void printSummary(int hits, int misses, int evictions);

void printUsage()
{
    printf("Usage: ./csim-ref [-hv]-s <s>-E <E>-b <b>-t <tracefile>\n");
    printf("-h: Optional help flag that prints usage info\n");
    printf("-v: Optional verbose flag that displays trace info");
    printf("-s <s>: Number ofset index bits (S = 2s is the number of sets)\n");
    printf("-E <E>: Associativity (number of lines per set)\n");
    printf("-b <b>: Number ofblock bits (B = 2b is the block size)\n");
    printf("-t <tracefile>:Nameof the valgrindtrace to replay\n");
}


typedef struct CacheLine {
    int valid; 
    int tag; // tag标记位
    int time; // 记录最后一次使用时间,在LRU算法中使用, time越大距离上一次使用的时间越长
} CacheLine_t;

typedef CacheLine_t* set_t; // CacheLine_t的数组为set

typedef set_t* cache_t;   // set_t的数组为cache


void updateTime(cache_t cache,int set_idx, int tag, int e_idx)
{
    cache[set_idx][e_idx].valid = 1;
    cache[set_idx][e_idx].tag = tag;
    for(int i = 0; i < E; i++){
        if(cache[set_idx][i].valid)
            cache[set_idx][i].time++;
    }
    cache[set_idx][e_idx].time = 0;
}

int findlru(cache_t cache, int set_idx)
{
    int max_time = 0;
    int res = 0; // 对于
    for(int i = 0; i < E; i++) {
        if(cache[set_idx][i].time > max_time) {
            max_time = cache[set_idx][i].time;
            res = i;
        }
    }
    return res;
}


void findCacheLine(cache_t cache,int set_idx, int tag)
{
    int isHit = 0, isFull = 1, idx = -1;
    for(int i = 0; i < E; i++) {
        // cache hit
        if(cache[set_idx][i].valid != 0 && cache[set_idx][i].tag == tag) {
            isHit = 1;
            hits++;
            updateTime(cache, set_idx, tag, i);
            if(verboseFlag) printf("hit ");
            return;
        }

        if(cache[set_idx][i].valid == 0 && isFull) {
            isFull = 0;
            idx = i;
        }
    }

    // cache miss or full
    if(!isHit) {
        misses++;
        if(verboseFlag) printf("miss ");
        if(isFull) {
            // lru替换策略
            int e_idx = findlru(cache, set_idx);
            evictions++;
            updateTime(cache, set_idx, tag, e_idx);
            if(verboseFlag) printf("eviction ");
        }
        else {
            updateTime(cache, set_idx, tag, idx);
        }
    }
}


int main(int argc, char *argv[])
{
    int opt = 0;
    while((opt = getopt(argc, argv, "s:E:b:t:hv")) != -1) {
        switch (opt) {
        case 's': 
            sscanf(optarg, "%d", &s);break;
        case 'E': 
            sscanf(optarg, "%d", &E);break;
        case 'b':
            sscanf(optarg, "%d", &b);break;
        case 't':
            tracefile = optarg;break;
        case 'h':
            printUsage();exit(EXIT_SUCCESS);
        case 'v':
            verboseFlag = 1;break;
        default:
            printUsage();exit(EXIT_FAILURE);
        }
    }

    // 检查参数
    FILE *file = fopen(tracefile, "r");
    if(s < 0 || E < 0 || b < 0 || file == NULL) {
        perror("ERROR");
        printUsage();
		exit(EXIT_FAILURE);
    }

    // Cache初始化，根据S,E,b构建cache[S][E]
    int S = (1<<s);
    cache_t cache = (cache_t)calloc(S, sizeof(set_t));
    for(int i = 0; i < S; i++) {
        cache[i] = (set_t)calloc(E, sizeof(CacheLine_t));
        for(int j = 0; j < E; j++) {
            cache[i][j].tag = -1;
            cache[i][j].valid = 0;
            cache[i][j].time = 0;
        }
    }

    // 读写Cache
    char op;
    uint64_t address;
    int size;
    // 从文件中输入
    while(fscanf(file," %c%lx,%d\n",&op, &address, &size) == 3) {

        if(verboseFlag) {
            printf("%c %lx,%d ", op, address, size);
        }

        int addr_tag = address >> (s + b);
        int addr_set_idx = (address >> b) & ((1 << s) - 1);

        switch (op)
        {
        case 'I':
            continue;
        case 'S':
            findCacheLine(cache, addr_set_idx, addr_tag);
            break;
        case 'L':
            findCacheLine(cache, addr_set_idx, addr_tag);
            break;
        case 'M':
            findCacheLine(cache, addr_set_idx, addr_tag);
            findCacheLine(cache, addr_set_idx, addr_tag);
            break;
        default:
            break;
        }
        if(verboseFlag) printf("\n");
    }

    // 打印统计数据
    printSummary(hits, misses, evictions);

    // 释放内存
    for(int i = 0; i < S; i++) free(cache[i]);
    free(cache);
    fclose(file);
    return 0;
}
