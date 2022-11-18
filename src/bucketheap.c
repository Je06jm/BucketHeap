#include <bucketheap.h>

#include "abstraction.h"
#include "config.h"

#include <stdint.h>
#include <stdio.h>

typedef struct bucketobject bucketobject_t;

typedef struct bucketlist {
    struct bucketlist* next;
    size_t obj_size;
    bucketobject_t* head;
} bucketlist_t;

struct bucketobject {
    struct bucketobject* next;
    bucketlist_t* list;
    bool boundary;
    uint8_t reserved[sizeof(uintptr_t) - sizeof(bool)];
};

typedef struct alloclist {
    struct alloclist* next;
    size_t count;
    uintptr_t allocs[];
} alloclist_t;

#define MAX_ALLOCLIST_COUNT ((BUCKET_HEAP_CONFIG_PAGE_ALLOC_SIZE_KB * 1024 - sizeof(alloclist_t)) / (sizeof(uintptr_t)))

bucketlist_t* buckets = NULL;
bucketheap_error_t error = BUCKET_HEAP_OK;

alloclist_t* allocs = NULL;

void bucketheap_init() {
    if (buckets != NULL) {
        error = BUCKET_HEAP_ERROR_WAS_NOT_INIT;
        return;
    }

    buckets = bucketheap_alloc_page(true);
    allocs = bucketheap_alloc_page(false);
    allocs->next = NULL;
    allocs->count = 0;
}

void bucketheap_finish() {
    if (buckets == NULL) {
        error = BUCKET_HEAP_ERROR_WAS_INIT;
        return;
    }

    {
        // Free all allocs
        alloclist_t* current = allocs;
        while (current != NULL) {
            for (size_t i = 0; i < MAX_ALLOCLIST_COUNT; i++) {
                bucketheap_free_page((void*)current->allocs[i], false);
            }

            alloclist_t* next = current->next;
            bucketheap_free_page(current, false);
            current = next;
        }
    }

    allocs = NULL;

    {
        // Free buckets
        bucketlist_t* current = buckets;
        while (current != NULL) {
            bucketlist_t* next = current->next;
            bucketheap_free_page(current, true);
            current = next;
        }
    }
}

void bucketheap_newobjects(bucketlist_t* list) {
    list->head = (bucketobject_t*)bucketheap_alloc_page(false);

    alloclist_t* current = allocs;
    while (current->count == MAX_ALLOCLIST_COUNT) {
        if (current->next == NULL) {
            current->next = (alloclist_t*)bucketheap_alloc_page(false);

            current->next->next = NULL;
            current->next->count = 0;
        }

        current = current->next;
    }

    current->allocs[current->count++] = (uintptr_t)list->head;

    size_t obj_size = list->obj_size + sizeof(bucketobject_t);
    size_t obj_count = (BUCKET_HEAP_CONFIG_PAGE_ALLOC_SIZE_KB * 1024) / obj_size;

    bucketobject_t* obj_last = NULL;
    bucketobject_t* obj_ptr = list->head;
    for (size_t i = 0; i < obj_count; i++) {
        obj_ptr->boundary = false;
        obj_ptr->list = list;
        
        if (obj_last != NULL) {
            obj_last->next = obj_ptr;
        }

        obj_last = obj_ptr;
        obj_ptr = (bucketobject_t*)((uintptr_t)obj_ptr + obj_size);
    }

    list->head->boundary = true;
}

void* bucketheap_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    if (size > BUCKET_HEAP_CONFIG_MAX_OBJECT_SIZE) {
        error = BUCKET_HEAP_ERROR_ALLOC_TO_BIG;
        return NULL;
    }

    // Find an appropriately sized bucket
    bucketlist_t* current = buckets;
    while (!(size < current->obj_size && (size<<1) > current->obj_size)) {
        if (current->next == NULL) {
            if ((current->obj_size>>1) < BUCKET_HEAP_CONFIG_MIN_OBJECT_SIZE) {
                break;
            }

            // Create new objects

            current->next = (bucketlist_t*)((uintptr_t)current + sizeof(bucketlist_t));
            current->next->obj_size = current->obj_size >> 1;

            bucketheap_newobjects(current->next);
        }

        current = current->next;
    }

    if (current->head == NULL) {
        bucketheap_newobjects(current);
    }

    bucketobject_t* obj = current->head;
    
    current->head = obj->next;

    return (void*)((uintptr_t)current + sizeof(bucketlist_t));
}

void bucketheap_free(void* pointer) {
    if (pointer == NULL) {
        error = BUCKET_HEAP_ERROR_CANNOT_FREE;
        return;
    }

    bucketobject_t* obj = (bucketobject_t*)((uintptr_t)pointer - sizeof(bucketlist_t));

    // Insert the object into the bucket using ascending order
    if (obj->list->head == NULL) {
        obj->list->head = obj;
        goto do_free_pages;
    }

    if (obj < obj->list->head) {
        obj->next = obj->list->head;
        obj->list->head = obj;
        goto do_free_pages;
    }

    bucketobject_t* current = obj->list->head;
    while (current->next != NULL) {
        if (obj < current->next) {
            obj->next = current->next;
            current->next = obj;
            goto do_free_pages;
        }

        current = current->next;
    }

    current->next = obj;

do_free_pages:;
    // See if we can free any pages

    size_t obj_size = obj->list->obj_size + sizeof(bucketobject_t);
    size_t obj_count = (BUCKET_HEAP_CONFIG_PAGE_ALLOC_SIZE_KB * 1024) / obj_size;

    bucketobject_t* end;
    bool can_free = true;

    current = obj->list->head;
    while (current != NULL) {
        if (current->boundary) {
            end = (bucketobject_t*)((uintptr_t)current + obj_count * obj_size);
            can_free = true;
            
            bucketobject_t* page = current;
            bucketobject_t* next;
            while (page != end) {
                next = (bucketobject_t*)((uintptr_t)page + obj_size);
                if (page->next != next) {
                    can_free = false;
                    break;
                }

                page = page->next;
            }

            if (can_free) {
                // Find in allocs

                alloclist_t* current_alloc = allocs;
                while (current_alloc != NULL) {
                    for (size_t i = 0; i < current_alloc->count; i++) {
                        if (current_alloc->allocs[i] == (uintptr_t)current) {
                            for (size_t j = i; j < current_alloc->count - 1; j++) {
                                current_alloc->allocs[j] = current_alloc->allocs[j+1];
                            }

                            current_alloc->count--;
                        }
                    }

                    current_alloc = current_alloc->next;
                }

                // Free page
                bucketheap_free_page((void*)current, false);
            }
        }

        current = current->next;
    }
}

bucketheap_error_t bucketheap_geterror() {
    bucketheap_error_t err = error;
    error = BUCKET_HEAP_OK;
    return err;
}

void bucketheap_errorstr(bucketheap_error_t err, char* str) {
    switch (err) {
        case BUCKET_HEAP_OK:
            sprintf(str, "BUCKET_HEAP_OK");
            break;
        
        case BUCKET_HEAP_ERROR_WAS_INIT:
            sprintf(str, "BUCKET_HEAP_ERROR_WAS_INIT");
            break;
        
        case BUCKET_HEAP_ERROR_WAS_NOT_INIT:
            sprintf(str, "BUCKET_HEAP_ERROR_WAS_NOT_INIT");
            break;
        
        case BUCKET_HEAP_ERROR_NO_MEMORY:
            sprintf(str, "BUCKET_HEAP_ERROR_NO_MEMORY");
            break;
        
        case BUCKET_HEAP_ERROR_ALLOC_TO_BIG:
            sprintf(str, "BUCKET_HEAP_ERROR_ALLOC_TO_BIG");
            break;
        
        case BUCKET_HEAP_ERROR_CANNOT_FREE:
            sprintf(str, "BUCKET_HEAP_ERROR_CANNOT_FREE");
            break;
        
        case BUCKET_HEAP_ERROR_PLATFORM:
            sprintf(str, "BUCKET_HEAP_ERROR_PLATFORM");
            break;
        
        default:
            sprintf(str, "UNKNOWN_ERROR");
            break;
    }
}