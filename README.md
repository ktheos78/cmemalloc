# cmemalloc

## About

This is a simple (and not particularly optimized) implementation of `malloc()`, `realloc()`, `calloc()` and `free()` from scratch, which I wrote to learn a thing or two about how memory allocators work. It works as follows:

We keep a linked list of memory blocks (which we mark as free or not free) which we keep extending if there is sufficient heap space and there are no free blocks in said list. If the heap cannot be extended any more *or* if the size of the allocation exceeds a certain (static, as opposed to glibc's implementation) threshold, the allocation is done through the `mmap()` system call. Because of this, we keep an `is_mmap` flag inside the block's header.

When freeing memory, if a block has been allocated through `mmap()` (i.e. its `is_mmap` flag is set to 1), we simply call `munmap()`. For non-mmap-allocated blocks, if the block is not at the program break, it is simply marked as free by updating its `is_free` flag. Otherwise, it is removed from the linked list, and the memory is released to the OS.

**Note**: `c_malloc()`, `c_realloc()`, `c_calloc()`, and `c_free()` are thread-safe (a global mutex is used to ensure atomicity).

## Use

When `make` is run, both a regular .o file as well as a .so file are made in `build/`, which you can then link into your own projects (you can also examine the compilation flags in the Makefile).