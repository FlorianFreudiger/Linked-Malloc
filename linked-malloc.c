#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

const size_t ALIGNMENT = 8;

struct LinkedMallocHeader {
    struct LinkedMallocHeader *prev;
    struct LinkedMallocHeader *next;
    size_t total_size; // Header + data size
};

// TODO: Mutex/Rwlock

struct LinkedMallocHeader *start = NULL;


void debug_print_header_chain() {
    struct LinkedMallocHeader *current = start;
    fprintf(stderr, "Start = %p\n", current);

    int i = 0;
    while (current != NULL) {
        fprintf(stderr, "%02d: prev=%p, current=%p, next=%p, size=%zu\n", i++,
                current->prev, current, current->next, current->total_size);

        // Catch loops
        if (current == current->next) {
            fprintf(stderr, "Loop detected!\n");
            break;
        }

        current = current->next;
    }

    fprintf(stderr, "Program break at %p\n\n", sbrk(0));
}


struct LinkedMallocHeader *malloc_ptr_to_header(void *ptr) {
    return ptr - sizeof(struct LinkedMallocHeader);
}

void *header_to_malloc_pointer(struct LinkedMallocHeader *header) {
    return (void *) header + sizeof(struct LinkedMallocHeader);
}

size_t calculate_required_size(size_t requested_size) {
    // Add header size
    size_t required_size = requested_size + sizeof(struct LinkedMallocHeader);

    // Round up size for alignment
    required_size += (ALIGNMENT - (required_size % ALIGNMENT)) % ALIGNMENT;

    return required_size;
}

// Add an initial block at the start, this simplifies freeing the first real block a lot
void malloc_initial() {
    size_t start_size = calculate_required_size(0);

    start = sbrk((intptr_t) start_size);
    start->prev = NULL;
    start->next = NULL;
    start->total_size = start_size;
}

void __attribute__((visibility("default"))) *malloc(size_t size) {
    if (size == 0) return NULL;

    if (start == NULL) malloc_initial();

    size_t required_space = calculate_required_size(size);

    // Find where to place header + data
    struct LinkedMallocHeader *previous_header = NULL;
    struct LinkedMallocHeader *destination_header = start;

    // Iterate through linked headers until we either find a gap big enough or we reach the end
    while (destination_header != NULL) {
        previous_header = destination_header;

        // Check if there's enough space between this and the next header
        if (destination_header->next != NULL) {
            void *current_data_end = (void *) destination_header + destination_header->total_size;
            size_t room = (void *) destination_header->next - current_data_end;
            if (room >= required_space) {
                destination_header = current_data_end;
                break;
            }
        }

        // If not, iterate to the next header
        destination_header = destination_header->next;
    }

    if (destination_header == NULL) {
        // End of the linked header list reached, ask for more space

        void *new_break = sbrk((intptr_t) required_space);
        if (new_break == (void *) -1) {
            fprintf(stderr, "sbrk syscall failed, error code: %d\n", errno);
        }

        destination_header = new_break;
    }

    // Modify last found and its next header to insert into chain:
    destination_header->next = previous_header->next;

    // First update next header, since the reference to it will be overwritten in the second step
    if (previous_header->next != NULL) {
        previous_header->next->prev = destination_header;
    }

    // Update last found header
    previous_header->next = destination_header;

    destination_header->prev = previous_header;
    destination_header->total_size = required_space;
    return header_to_malloc_pointer(destination_header);
}

void __attribute__((visibility("default"))) free(void *ptr) {
    if (ptr == NULL) return;

    struct LinkedMallocHeader *header = malloc_ptr_to_header(ptr);

    // Connect previous header to the header after this one
    header->prev->next = header->next;

    if (header->next != NULL) {
        // Free by connecting previous and next header to another, "skipping" element in chain
        header->next->prev = header->prev;

    } else {
        // If header is the end of the chain free the excess memory
        sbrk(-(intptr_t) header->total_size);
    }
}

void __attribute__((visibility("default"))) *calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;

    // TODO: Handle overflows
    size_t total_size = nmemb * size;

    void *ptr = malloc(total_size);

    // Zero content
    for (size_t i = 0; i < nmemb; ++i) {
        memset(ptr + i * size, 0, size);
    }

    return ptr;
}

void __attribute__((visibility("default"))) *realloc(void *ptr, size_t size) {
    if (ptr == NULL) return malloc(size);

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    struct LinkedMallocHeader *header = malloc_ptr_to_header(ptr);

    size_t old_total_size = header->total_size;
    size_t new_total_size = calculate_required_size(size);

    if (new_total_size == old_total_size) {
        // No action needed
        return ptr;
    }

    // Simple resize possible if...
    bool can_resize_in_place =
            //... no extra space needed
            new_total_size < old_total_size

            //... at the end of the chain
            || header->next == NULL

            //... there is a large enough gap to the next header
            || (void *) header + new_total_size <= (void *) header->next;

    if (can_resize_in_place) {
        // Update size
        header->total_size = new_total_size;

        // If at end: Reserve or free size change
        if (header->next == NULL) {
            sbrk((intptr_t) size - (intptr_t) old_total_size);
        }

        return ptr;
    }

    // Else allocate new block, copy data over and free old block
    void *new_ptr = malloc(size);
    memcpy(new_ptr, ptr, size);
    free(ptr);

    return new_ptr;
}
