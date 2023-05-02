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

void *start = NULL;

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

void *malloc(size_t size) {
    if (size == 0) return NULL;

    size_t required_space = size + sizeof(struct LinkedMallocHeader);

    // Round up size to ensure alignment
    required_space += (ALIGNMENT - (required_space % ALIGNMENT)) % ALIGNMENT;

    // Find where to place header + data
    struct LinkedMallocHeader *previous_header = NULL;
    struct LinkedMallocHeader *destination_header = start;

    // Iterate through linked headers until we either find a gap big enough or we reach the end
    while (destination_header != NULL) {

        // Check if there's enough space between this and the next header
        if (destination_header->next != NULL) {
            void *current_data_end = (void *) destination_header + destination_header->total_size;
            size_t room = (void *) destination_header->next - current_data_end;
            if (room >= required_space) {
                destination_header = current_data_end;
                // fprintf(stderr, "Found space, room=%zu, required=%zu\n", room, required_space);
                break;
            }
        }

        // If not, iterate to the next header
        previous_header = destination_header;
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

    destination_header->prev = previous_header;
    destination_header->total_size = required_space;

    // Modify last found and its next header to insert into chain:
    if (previous_header != NULL) {
        destination_header->next = previous_header->next;

        // First update next header, since the reference to it will be overwritten in the second step
        if (previous_header->next != NULL) {
            previous_header->next->prev = destination_header;
        }

        // Update last found header
        previous_header->next = destination_header;

    } else {
        destination_header->next = NULL;

        // If this is the very first header update start
        start = destination_header;
    }

    return (void *) destination_header + sizeof(struct LinkedMallocHeader);
}

void free(void *ptr) {
    if (ptr == NULL) return;

    struct LinkedMallocHeader *header = ptr - sizeof(struct LinkedMallocHeader);

    if (header->prev != NULL) {
        header->prev->next = header->next;

        if (header->next != NULL) {
            // Free by connecting previous and next header to another, "skipping" element in chain
            header->next->prev = header->prev;

        } else {
            // If header is the end of the chain free the excess memory
            sbrk(-(intptr_t) header->total_size);
        }
    } else {
        if (header->next != NULL) {
            // By setting size to 0 allow element it to be overwritten
            header->total_size = 0;

        } else {
            // If header is the end of the chain free the excess memory
            sbrk(-(intptr_t) header->total_size);

            // If this also was the first header reset start
            start = NULL;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    // TODO: Handle overflows
    size_t total_size = nmemb * size;

    void *ptr = malloc(total_size);
    memset(ptr, 0, total_size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) return malloc(size);

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    struct LinkedMallocHeader *header = ptr - sizeof(struct LinkedMallocHeader);
    size_t old_size = header->total_size - sizeof(struct LinkedMallocHeader);

    if (size == old_size) {
        // No action needed
        return ptr;
    }

    size_t new_total_size = size + sizeof(struct LinkedMallocHeader);

    // Simple resize possible if...
    bool can_resize_in_place =
            //... no extra space needed
            size < old_size

            //... at the end of the chain
            || header->next == NULL

            //... there is a large enough gap to the next header
            || (void *) header + new_total_size <= (void *) header->next;


    if (can_resize_in_place) {
        // Update size
        header->total_size = new_total_size;

        // If at end: Reserve or free size change
        if (header->next == NULL) {
            sbrk((intptr_t) size - (intptr_t) old_size);
        }

        return ptr;
    }

    // Else allocate new block, copy data over and free old block
    void *new_ptr = malloc(size);
    memcpy(new_ptr, ptr, size);
    free(ptr);

    return new_ptr;
}
