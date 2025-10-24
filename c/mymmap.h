// faster alloc/free for large buffer

#pragma once

// uint64_t
#include <stdint.h>
// exit()
#include <stdlib.h>
// perror
#include <stdio.h>
// O_APPEND
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// mmap
#include <sys/mman.h>
// unlink
#include <unistd.h>

#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)

#define twoMB (2*1024*1024)
#define oneGB (1024*1024*1024)

void *mymap(uint64_t *size);
void myunmap(void *ptr, uint64_t size);
