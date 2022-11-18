#ifndef BUCKET_HEAP_H
#define BUCKET_HEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef enum bucketheap_error {
    BUCKET_HEAP_OK,
    BUCKET_HEAP_ERROR_WAS_INIT,
    BUCKET_HEAP_ERROR_WAS_NOT_INIT,
    BUCKET_HEAP_ERROR_NO_MEMORY,
    BUCKET_HEAP_ERROR_ALLOC_TO_BIG,
    BUCKET_HEAP_ERROR_CANNOT_FREE,
    BUCKET_HEAP_ERROR_PLATFORM
} bucketheap_error_t;

void bucketheap_init();
void bucketheap_finish();

void *bucketheap_alloc(size_t size);
void bucketheap_free(void* pointer);

bucketheap_error_t bucketheap_geterror();
void bucketheap_errorstr(bucketheap_error_t err, char* str);

#ifdef __cplusplus
}

namespace bucketheap {

    inline void* operator new(size_t size) {
        return bucketheap_alloc(size);
    }

    inline void* operator new[](size_t size) {
        return bucketheap_alloc(size);
    }

    inline void operator delete(void* pointer) {
        bucketheap_free(pointer);
    }

    inline void operator delete[](void* pointer) {
        bucketheap_free(pointer);
    }

}
#endif

#endif