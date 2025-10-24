// faster alloc/free for in-memory buffer, also supports file-backed buffer

#include "mymmap.h"

void *mymap(uint64_t *size) {
#ifdef FILEBACKED
  /*if (*size % twoMB) {
   *size = ((size/twoMB) +1) * twoMB;
   }*/

  char fname[] = "/mnt/media/deletemeXXXXXX";

  int tfd = mkstemp(fname);

  if (-1 == tfd) {
    perror("mkstemp failed: ");
    exit(1);
  }

  int err = unlink(fname);

  if (-1 == err) {
    perror("unlink failed: ");
    exit(1);
  }

  off_t foo = lseek(tfd, *size, SEEK_CUR);

  if (-1 == foo) {
    perror("seek failed: ");
    exit(1);
  }

  foo = write(tfd, " ", 1);

  void* temp = mmap(NULL, *size, PROT_WRITE, MAP_NORESERVE|MAP_SHARED, tfd, 0);
#else
  uint64_t rounding = 1;

  if (rounding && *size % rounding) {
    // adjust length for myunmap to page alignment
    *size = ((*size/rounding) +1) * rounding;
  }

  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;

  // XXX test for Huge TLB (MAP_HUGETLB)?
  // using big pages without MAP_HUGETLB benchmarks worse than no rounding
  if (rounding == oneGB) {
    mmap_flags |= MAP_HUGE_1GB;
  } else if (rounding == twoMB) {
    mmap_flags |= MAP_HUGE_2MB;
  }

  void *temp = mmap(NULL, *size, PROT_WRITE, mmap_flags, -1, 0);
#endif

  if (MAP_FAILED == temp) {
    perror("mmap failed: ");
    exit(1);
  }

  return temp;
}

void myunmap(void *ptr, uint64_t size) {
  int err = munmap(ptr, size);
  if (0 != err) {
    perror("unmap failed: ");
    exit(1);
  }
}
