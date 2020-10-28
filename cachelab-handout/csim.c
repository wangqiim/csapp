#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "cachelab.h"
typedef long unsigned int uint64;

#define Debug() { printf("wwwwqqqqq\n"); };
#define IN() { printf("-------function in------\n"); }
#define OUT() { printf("-------function out------\n"); }

uint64 cache_s;
uint64 cache_E;
uint64 cache_b;
uint64 ***cache;  //[i][j][0] : Valid bit; [i][j][1] : Tag; [i][j][2] : LRU counter,
int _hits = 0, _misses = 0, _evictions = 0;
int verbose = 0;
char* ans[3] = {"miss", "miss eviction", "hit"};

void set_sEb_cache(uint64 s, uint64 E, uint64 b) {
    cache_s = s;
    cache_E = E;
    cache_b = b;
    s = (1u << cache_s);
    cache = (uint64 ***)malloc(s * sizeof(uint64**));
    for (uint64 i = 0; i < s; i++)
        *(cache + i) = (uint64 **)malloc(cache_E * sizeof(uint64*));
    for (uint64 i = 0; i < s; i++)
        for (uint64 j = 0; j < cache_E; j++)
            *(*(cache + i) + j) = (uint64 *)malloc(3 * sizeof(uint64));
    for (uint64 i = 0; i < s; i++)
        for (uint64 j = 0; j < cache_E; j++)
            cache[i][j][0] = 0;
}

void free_cache() {
    for (uint64 i = 0; i < (1u << cache_s); i++) {
        for (uint64 j = 0; j < cache_E; j++)
            free(*(*(cache + i) + j));
        free(*(cache + i));
    }
    free(cache);
}

int T = 1;
//0miss ，1 miss_evictions, 2 hit
int LRU(uint64 address) {
    uint64 tag = address >> (cache_s + cache_b);
    uint64 s = (address >> cache_b) & ((1u << cache_s) - 1);
    int ishit = 0;
    for (uint64 j = 0; j < cache_E; j++) {    //判断是否命中
        if (cache[s][j][0] == 1 && cache[s][j][1] == tag) { 
            cache[s][j][2] = 0;
            _hits++;
            ishit = 2;
            break;
        }
    }
    if (ishit != 2) {   //未命中，则替换
        _misses++;
        unsigned index = -1;    //选择位置
        unsigned cnt = 0;
        for (uint64 j = 0; j < cache_E; j++) {
            if (cache[s][j][0] == 0) {
                index = j;
                break;
            }
            if (cache[s][j][2] > cnt) {
                index = j;
                cnt = cache[s][j][2];
            }
        }
        if (cache[s][index][0] == 1) {
            _evictions++;
            ishit = 1;
        }
        cache[s][index][0] = 1;
        cache[s][index][1] = tag;
        cache[s][index][2] = 0;
    }
    for (uint64 j = 0; j < cache_E; j++) //计数器+1
        cache[s][j][2]++;
    T++;
    return ishit;
}

void printHelp() {
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

char* read_arg(int argc, char* argv[]) {
    int opt;
    uint64 s, E, b;
    s = E = b = 0;
    char* path;
    while (-1 != (opt = getopt(argc, argv, "hvs:E:b:t:"))) {
        switch (opt) {
            case 'h':
                printHelp(); break;
            case 'v':
                verbose = 1; break;
            case 's':
                s = (uint64)atoll(optarg); break;
            case 'E':
                E = (uint64)atoll(optarg); break;
            case 'b':
                b = (uint64)atoll(optarg); break;
            case 't':
                path = optarg; break;
            default:
                printHelp(); break;
        }
    }
    if (s == 0 || E == 0 || b == 0) {
        printHelp();
        exit(0);
    }
    set_sEb_cache(s, E, b);
    return path;
}

void read_file(char *path) {
    FILE * pFile;
    char operation;
    uint64 address;
    uint64 size;
    pFile = fopen(path, "r");
    while (fscanf(pFile, " %c %lx,%lu", &operation, &address, &size) > 0) {
        if (operation == 'I') continue;
        if (verbose == 1) {
            if (operation == 'M')
                printf("%c %lx,%lu %s %s\n", operation, address, size, ans[LRU(address)], ans[LRU(address)]);
            else
                printf("%c %lx,%lu %s\n", operation, address, size, ans[LRU(address)]);
        } else {
            if (operation == 'M')
                LRU(address);
            LRU(address);
        }
    }
    fclose(pFile);
}

int main(int argc, char* argv[]) {
    char *path = read_arg(argc, argv);
    read_file(path);
    printSummary(_hits, _misses, _evictions);
    free_cache();
	return 0;
}
