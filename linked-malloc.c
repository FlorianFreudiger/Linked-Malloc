#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

// TODO: Alignment

struct LinkedMallocHeader {
    struct LinkedMallocHeader *previous;
    struct LinkedMallocHeader *next;
    size_t total_size; // Header + data size
};

// TODO: Mutex/Rwlock

void *start = NULL;

void *malloc(size_t size) {
    if (size == 0) return NULL;

    size_t required_space = size + sizeof(struct LinkedMallocHeader);

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
        // We have reached the end of the linked header list

        void *new_break = sbrk(required_space);
        if (new_break == (void *) -1) {
            fprintf(stderr, "sbrk syscall failed, error code: %d\n", errno);
        }

        struct LinkedMallocHeader *new_header = new_break;
        new_header->previous = previous_header;
        new_header->next = NULL;
        new_header->total_size = required_space;

        // Update previous header if it exists, if not then this was the very first element
        if (previous_header != NULL) {
            previous_header->next = new_header;
        } else {
            start = new_header;
        }

        return (void *) new_header + sizeof(struct LinkedMallocHeader);
    }

    // Found enough space between 2 headers
    fprintf(stderr, "This should not happen, not yet.\n");
    return 0;
}

void free(void *ptr) {
    if (ptr == NULL) return;
}
