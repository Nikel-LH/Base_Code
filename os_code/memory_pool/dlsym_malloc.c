#define _GNU_SOURCE
#include <dlfcn.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
 
typedef void *(*malloc_t)(size_t size);
malloc_t malloc_f;
 
typedef void (*free_t)(void *p);
free_t free_f;
 
 
int enable_malloc_hook = 1;
 
int enable_free_hook = 1;
 
void *malloc(size_t size) {
 
    if (enable_malloc_hook) {
        enable_malloc_hook = 0;
 
        void *p = malloc_f(size);
 
        void *caller = __builtin_return_address(0);
 
        char buff[128] = {0};
        sprintf(buff, "./mem/%p.mem", p);
 
        FILE *fp = fopen(buff, "w");
        fprintf(fp, "[+%p]malloc --> addr:%p size:%lu\n", caller, p, size);
        fflush(fp);
 
        enable_malloc_hook = 1;
        return p;
    } else {
 
        return malloc_f(size);
 
    }
    return NULL;
}
 
void free(void *p) {
 
    if (enable_free_hook) {
        enable_free_hook = 0;
        char buff[128] = {0};
        sprintf(buff, "./mem/%p.mem", p);
 
        if (unlink(buff) < 0) {
            printf("double free: %p\n", p);
        }
 
        free_f(p);
 
 
        enable_free_hook = 1;
    } else {
        free_f(p);
    }
 
}
 
static int init_hook() {
 
    malloc_f = dlsym(RTLD_NEXT, "malloc");
 
    free_f = dlsym(RTLD_NEXT, "free");
 
}
 

int main() {
 
    init_hook();
 
    void *p1 = malloc(10);
    void *p2 = malloc(20);
 
    free(p1);
 
    void *p3 = malloc(30);
    void *p4 = malloc(40);
 
    free(p2);
    free(p4);
 
    return 0;
 
}