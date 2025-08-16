#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include "cmemalloc.h"

// global pointers to keep track of head and tail
static header_t *head = NULL, *tail = NULL;

// threshold for allocating using mmap, initialized at 64KB
static size_t MMAP_THRESHOLD = 1 << 16;

static pthread_mutex_t g_memaccess_lock = PTHREAD_MUTEX_INITIALIZER;

size_t align_size(size_t size)
{
    return (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
}

void *c_malloc(size_t size)
{
    size_t total_size;
    void *block;
    header_t *header;

    if (!size)
        return NULL;

    // start of critical section //
    pthread_mutex_lock(&g_memaccess_lock);

    // align size to sizeof(void *)
    size = align_size(size);

    if (size >= MMAP_THRESHOLD)
    {
        total_size = size + sizeof(header_t);
        block = mmap(NULL, total_size, 
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (block == MAP_FAILED)
        {
            pthread_mutex_unlock(&g_memaccess_lock);
            return NULL;
        }

        // init block data
        header = (header_t *)block;
        header->b.size = size;
        header->b.is_free = 0;
        header->b.is_mmap = 1;
        header->b.next = NULL;

        pthread_mutex_unlock(&g_memaccess_lock);
        return (void *)(header + 1);
    }

    header = get_next_free_block(size);
    if (header)
    {
        header->b.is_free = 0;
        pthread_mutex_unlock(&g_memaccess_lock);
        return (void *)(header + 1);
    }

    total_size = size + sizeof(header_t);
    
    // if no heap memory is available, try mmap
    block = sbrk(total_size);
    if (block == (void *)-1)
    {
        block = mmap(NULL, total_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1 ,0);

        if (block == MAP_FAILED)
        {
            pthread_mutex_unlock(&g_memaccess_lock);
            return NULL;
        }

        // init block data
        header = (header_t *)block;
        header->b.size = size;
        header->b.is_free = 0;
        header->b.is_mmap = 1;
        header->b.next = NULL;

        pthread_mutex_unlock(&g_memaccess_lock);
        return (void *)(header + 1);
    }

    // assign block to header
    header = (header_t *)block;
    header->b.size = size;
    header->b.is_free = 0;
    header->b.is_mmap = 0;
    header->b.next = NULL;
    
    // update head & tail
    if (!head)
        head = header;
    if (tail)
        tail->b.next = header;
    tail = header;

    pthread_mutex_unlock(&g_memaccess_lock);
    // end of critical section //

    return (void *)(header + 1);
}

void *c_realloc(void *ptr, size_t size)
{
    header_t *header;
    void *res;

    if (!ptr || !size)
        return NULL;

    // align size to sizeof(void *)
    size = align_size(size);

    header = (header_t *)ptr - 1;
    if (header->b.size >= size)
        return ptr;

    res = c_malloc(size);
    if (res)
    {
        memcpy(res, ptr, header->b.size);
        c_free(ptr);
    }

    return res;
}

void *c_calloc(size_t nmemb, size_t size)
{
    size_t total_size = nmemb * size;
    void *res;

    if (!nmemb || !size)
        return NULL;
    
    // detect overflow
    if (nmemb != total_size / size)
        return NULL;

    // align size to sizeof(void *)
    total_size = align_size(total_size);

    res = c_malloc(total_size);
    if (!res)
        return NULL;

    // zero-set and return
    memset(res, 0, total_size);
    return res;
}

void c_free(void *ptr)
{
    header_t *header, *tmp;
    void *prog_break;

    if (!ptr)
        return;

    // start of critical section //
    pthread_mutex_lock(&g_memaccess_lock);

    header = (header_t *)ptr - 1;

    // check if mmap'd
    if (header->b.is_mmap == 1)
    {
        int unmap = munmap((void *)header, header->b.size + sizeof(header_t));
        if (unmap == -1)
            perror("munmap");

        pthread_mutex_unlock(&g_memaccess_lock);
        return;
    }

    prog_break = sbrk(0);

    // if block at program break, release to OS
    if ((uint8_t *)header + sizeof(header_t) + header->b.size == (uint8_t *)prog_break)
    {
        if (head == tail)
            head = tail = NULL;

        else
        {
            tmp = head;
            while (tmp)
            {
                if (tmp->b.next == tail)
                {
                    tmp->b.next = NULL;
                    tail = tmp;
                    break;
                }

                tmp = tmp->b.next;
            }

            sbrk(0 - sizeof(header_t) - header->b.size);

            pthread_mutex_unlock(&g_memaccess_lock);
            return;
        }
    }

    // otherwise mark as free
    header->b.is_free = 1;

    // end of critical section //
    pthread_mutex_unlock(&g_memaccess_lock);
} 

static header_t *get_next_free_block(size_t size)
{
    header_t *h = head;
    while (h)
    {
        if (h->b.size >= size && h->b.is_free == 1)
            return h;

        h = h->b.next;
    }

    return NULL;
}