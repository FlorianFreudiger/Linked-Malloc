# Linked-Malloc
A malloc implementation using a simple doubly linked list layout.

## Allocation policy
Each allocated block is placed as an element of a chain.
A small 3x 8 byte (for 64-bit systems) header is used to store pointers to the next and previous headers in the chain
and the size of the allocation.

Allocating new memory is performed by finding a gap which fits the block between 2 existing elements of the chain.
If none is found the block will be appended to the end of the chain.

Memory is freed by "skipping" the element in the chain, the previous and next element will be linked to another.
This creates a gap between the blocks, which future malloc calls can fill.

The program break is continually adjusted using `sbrk` to be right after the last element in the chain.
The last element can be identified since its next field is set to NULL.

Block sizes are round up to nearest alignment multiple.
The alignment is set to 8 bytes by default but can be changed inside linked-malloc.c.
To disable alignment set it to 1.

## Usage
Compile linked-malloc via make.

Use the LD_PRELOAD trick in order to give precedence to linked-malloc over glibc
to load the malloc, free, calloc and realloc functions in an application.
```
# For single commands:
$ LD_PRELOAD=/.../linked-malloc.so <application>

# To set it for multiple commands:
$ LD_PRELOAD=/.../linked-malloc.so
$ <application 1>
$ <application 2>
$ ...
```
Make sure to specify the path to `linked-malloc.so` is absolute,
to cover child processes using a different working directory.

## Future possible improvements
- Keep track of the largest previous gap and of the chain-end to skip iteration for larger sizes
- Keep buffer between chain and program break to avoid calling sbrk on every malloc or free of the last element
- Consolidate gaps between blocks when realloc is resizing in place
- Track gaps in an interwoven linked list in order to find gaps faster
