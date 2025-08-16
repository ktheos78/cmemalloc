#ifndef CMEMALLOC_H_
#define CMEMALLOC_H_

typedef union header
{
    struct
    {
        size_t size;
        uint8_t is_free;
        uint8_t is_mmap;
        union header *next;
    } b;

    // align to 16 bytes
    uint8_t align[16];      
} header_t;

size_t align_size(size_t size);

void *c_malloc(size_t size);
void *c_realloc(void *ptr, size_t size);
void *c_calloc(size_t nmemb, size_t size);
void c_free(void *ptr);

static header_t *get_next_free_block(size_t size);

#endif  /* CMEMALLOC_H_ */