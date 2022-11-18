#if defined(UNIX) || defined(__unix__)

#include "abstraction.h"
#include "config.h"

#include <sys/mman.h>
#include <strings.h>
#include <errno.h>

#include <stdio.h>

void* bucket_heap_alloc_page(bool force_4kb) {
    void* page;
    page = mmap(
        NULL,
        force_4kb ? 0x1000 : BUCKET_HEAP_CONFIG_PAGE_ALLOC_SIZE_KB * 1024,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );

    if (page == MAP_FAILED) {
        if (errno == ENOMEM) {
            // Out of memory
            return NULL;
        }

        fprintf(stderr, "MMap error: ");

        switch (errno) {
            case EACCES: fprintf(stderr, "EACCES"); break;
            case EAGAIN: fprintf(stderr, "EAGAIN"); break;
            case EBADF: fprintf(stderr, "EBADF"); break;
            case EINVAL: fprintf(stderr, "EINVAL"); break;
            case ENFILE: fprintf(stderr, "ENFILE"); break;
            case ENODEV: fprintf(stderr, "ENODEV"); break;
            case EPERM: fprintf(stderr, "EPERM"); break;
            case ETXTBSY: fprintf(stderr, "ETXTBSY"); break;
            case EOVERFLOW: fprintf(stderr, "EOVERFLOW"); break;
            default: fprintf(stderr, "Unknown %u", error); break;
        }

        fprintf(stderr, "\n");

        return ONE;
    }

    return page;
}

void bucket_heap_free_page(void* pointer, bool force_4k) {
    munmap(pointer, force_4k ? 0x1000 : BUCKET_HEAP_CONFIG_PAGE_ALLOC_SIZE_KB * 1024)
}

#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

#include "abstraction.h"
#include "config.h"

#include <Windows.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

void* bucket_heap_alloc_page(bool force_4kb) {
    void* ptr = VirtualAlloc(
        NULL,
        force_4kb ? 0x1000 : BUCKET_HEAP_CONFIG_PAGE_ALLOC_SIZE_KB * 1024,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (ptr == NULL) {
        uint32_t err = GetLastError();
        
        switch (err) {
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_OUTOFMEMORY:
            case ERROR_COMMITMENT_LIMIT:
                // Out of memory
                return NULL;
            default:
                fprintf(stderr, "VirtualAlloc error: %u\n", err);
                return ONE;
        }
    }

    return ptr;
}

void bucket_heap_free_page(void* pointer, bool force_4k) {
    VirtualFree(
        pointer,
        force_4k ? 0x1000 : BUCKET_HEAP_CONFIG_PAGE_ALLOC_SIZE_KB * 1024,
        MEM_DECOMMIT | MEM_RELEASE
    );
}

#endif