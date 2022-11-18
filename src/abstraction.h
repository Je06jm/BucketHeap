#ifndef BUCKET_HEAP_ABSTRACTION_H
#define BUCKET_HEAP_ABSTRACTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#ifndef ONE
#define ONE ((void*)1)
#endif

void* bucketheap_alloc_page(bool force_4kb);
void bucketheap_free_page(void* pointer, bool force_4k);

#ifdef __cplusplus
}
#endif

#endif