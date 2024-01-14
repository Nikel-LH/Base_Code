

#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <mcheck.h>

#include <malloc.h>

#if 0

void *_malloc(size_t size, const char *filename, int line) {

	void *p = malloc(size);

	char buff[128] = {0};
	sprintf(buff, "./mem/%p.mem", p);

	FILE *fp = fopen(buff, "w");
	fprintf(fp, "[+]%s:%d, addr: %p, size: %ld\n", 
		filename, line, p, size);

	fflush(fp);
	fclose(fp);
	//printf("[+]%s:%d, %p\n", filename, line, p);

	return p;

}

void _free(void *p, const char *filename, int line) {

	//printf("[-]%s:%d, %p\n", filename, line, p);

	char buff[128] = {0};
	sprintf(buff, "./mem/%p.mem", p);

	if (unlink(buff) < 0) {
		printf("double free: %p\n", p);
		return ;
	}
	
	return free(p);

}

#define malloc(size)   _malloc(size, __FILE__, __LINE__)

#define free(size)   _free(size, __FILE__, __LINE__)

#elif 1

typedef void *(*malloc_t)(size_t size);
typedef void (*free_t)(void *ptr);

malloc_t malloc_f = NULL;
free_t free_f = NULL;


int enable_malloc_hook = 1;
int enable_free_hook = 1;


void* ConvertToELF(void *addr) {

	Dl_info info;
	struct link_map *link;

	dladdr1(addr, &info, (void **)&link, RTLD_DL_LINKMAP);

	return (void*)((size_t)addr - link->l_addr);

}



// main --> func --> f();
// 

//
void *malloc(size_t size) {

	void *p = NULL;

	if (enable_malloc_hook) {
		enable_malloc_hook = 0;
		
		//printf("size: %ld\n", size);
		p = malloc_f(size);

		void *caller = __builtin_return_address(0);
		
		
		char buff[128] = {0};
		sprintf(buff, "./mem/%p.mem", p);
	
		FILE *fp = fopen(buff, "w");
		fprintf(fp, "[+]%p, addr: %p, size: %ld\n", 
			ConvertToELF(caller), p, size);

		fflush(fp);

		enable_malloc_hook = 1;
	} else {

		p = malloc_f(size);

	}

	return p;
}

void free(void *ptr) {

	if (enable_free_hook) {
		enable_free_hook = 0;
		
		char buff[128] = {0};
		sprintf(buff, "./mem/%p.mem", ptr);

		if (unlink(buff) < 0) {
			printf("double free: %p\n", ptr);
			return ;
		}
		free_f(ptr);
		
		enable_free_hook = 1;
	} else {

		free_f(ptr);

	}

}




static void init_hook(void) {

	if (malloc_f == NULL) {
		malloc_f = (malloc_t)dlsym(RTLD_NEXT, "malloc");
	}

	
	if (free_f == NULL) {
		free_f = (free_t)dlsym(RTLD_NEXT, "free");
	}

}

#elif 0

extern void *__libc_malloc(size_t size);

extern void __libc_free(void *p);


int enable_malloc_hook = 1;
int enable_free_hook = 1;


void *malloc(size_t size) {

	void *p = NULL;

	if (enable_malloc_hook) {
		enable_malloc_hook = 0;
		
		//printf("size: %ld\n", size);
		p = __libc_malloc(size);

		void *caller = __builtin_return_address(0);

		
		char buff[128] = {0};
		sprintf(buff, "./mem/%p.mem", p);
	
		FILE *fp = fopen(buff, "w");
		fprintf(fp, "[+]%p, addr: %p, size: %ld\n", 
			caller, p, size);

		fflush(fp);

		enable_malloc_hook = 1;
	} else {

		p = __libc_malloc(size);

	}

	return p;
}

void free(void *ptr) {

	if (enable_free_hook) {
		enable_free_hook = 0;
		
		char buff[128] = {0};
		sprintf(buff, "./mem/%p.mem", ptr);

		if (unlink(buff) < 0) {
			printf("double free: %p\n", ptr);
			return ;
		}
		__libc_free(ptr);
		
		enable_free_hook = 1;
	} else {

		__libc_free(ptr);

	}

}

#elif 0


typedef void *(*malloc_t)(size_t size);
typedef void (*free_t)(void *ptr);

malloc_t malloc_f = NULL;
free_t free_f = NULL;

void mem_trace(void);
void mem_untrace(void);



void *_malloc_hook_f(size_t size, const char *filename, int line) {

	mem_untrace();

	void *p = malloc(size);

	char buff[128] = {0};
	sprintf(buff, "./mem/%p.mem", p);

	FILE *fp = fopen(buff, "w");
	fprintf(fp, "[+]%s:%d, addr: %p, size: %ld\n", 
		filename, line, p, size);

	fflush(fp);
	fclose(fp);
	//printf("[+]%s:%d, %p\n", filename, line, p);

	mem_trace();

	return p;

}

void _free_hook_f(void *p, const char *filename, int line) {

	//printf("[-]%s:%d, %p\n", filename, line, p);

	mem_untrace();

	char buff[128] = {0};
	sprintf(buff, "./mem/%p.mem", p);

	if (unlink(buff) < 0) {
		printf("double free: %p\n", p);
		return ;
	}
	
	free(p);

	mem_trace();

}




void mem_trace(void) {

	malloc_f = __malloc_hook;
	free_f = __free_hook;

	__malloc_hook = _malloc_hook_f;
	__free_hook = _free_hook_f;

}


void mem_untrace(void) {

	__malloc_hook = malloc_f;
	__free_hook = free_f;

}



#endif


// export MALLOC_TRACE=./mem.txt
int main() {

	//mtrace();
	//setenv("MALLOC_TRACE", "./mem.txt");

	init_hook();

	void *p1 = malloc(10);  //_malloc(size, __FILE__, __LINE__)
	void *p2 = malloc(15);
	void *p3 = malloc(20);

	free(p2);
	free(p3);

	//muntrace();

}



