// Pointer-chasing latency by buffer size, pinned to one CPU: the steps are the L1, L2 and L3 sizes (a cross-check of ccsidr_dump.c).
//   aarch64-linux-gnu-gcc -O2 -static -o cache_latency cache_latency.c   then   ./cache_latency <cpu>   (0 is a Silver core, 7 a Gold one)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sched.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e9+t.tv_nsec;}
int main(int argc,char**argv){
  int cpu=atoi(argv[1]); cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu,&s); sched_setaffinity(0,sizeof s,&s);
  size_t kb[]={4,8,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,8192,16384};
  srand(1);
  for(unsigned k=0;k<sizeof kb/sizeof*kb;k++){
    size_t bytes=kb[k]*1024, n=bytes/64; char*buf=aligned_alloc(4096,bytes);
    uint32_t*perm=malloc(n*sizeof*perm); for(size_t i=0;i<n;i++)perm[i]=i;
    for(size_t i=n-1;i>0;i--){size_t j=rand()%(i+1);uint32_t t=perm[i];perm[i]=perm[j];perm[j]=t;}
    for(size_t i=0;i<n;i++)*(void**)(buf+(size_t)perm[i]*64)=buf+(size_t)perm[(i+1)%n]*64;
    void*p=buf+(size_t)perm[0]*64; size_t iters=20000000;
    for(size_t i=0;i<n*2&&i<iters;i++)p=*(void**)p; asm volatile("" : : "r"(p) : "memory");
    double t0=now(); for(size_t i=0;i<iters;i++)p=*(void**)p; asm volatile("" : : "r"(p) : "memory"); double t1=now();
    printf("%6zu KB  %6.2f ns\n",kb[k],(t1-t0)/iters); (void)p; free(buf); free(perm);
  }
  return 0;}
