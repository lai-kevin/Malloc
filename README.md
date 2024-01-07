# x86-64 Dynamic Memory Allocator

## Overview

This is a dynamic memory allocator implemented in C for x86-64 architecture. It was developed as part of a homework assignment, focusing on efficient memory management. The allocator employs segregated free lists and "quick lists" to improve time complexity, and it features immediate coalescing of large blocks with adjacent free blocks on free operations. Additionally, the allocator ensures that blocks are split without generating splinters and omits block footers to improve space efficiency.

## Features

- Segregated free lists for different block sizes to enhance allocation efficiency.
- Utilization of "quick lists" to further optimize memory allocation times.
- Immediate coalescing of large blocks with adjacent free blocks on free operations.
- Splitting of blocks without generating splinters to prevent wasted memory.
- Omission of block footers for improved space efficiency.

### Segregated Free Lists

The allocator maintains separate free lists for different block sizes, allowing for more efficient allocation by reducing fragmentation.

### Quick Lists

"Quick lists" are utilized to speed up the allocation process for frequently requested block sizes.

### Immediate Coalescing

Large blocks are immediately coalesced with adjacent free blocks upon freeing to prevent fragmentation.

### Block Splitting Without Splinters

Blocks are split without generating splinters, ensuring that the allocator efficiently uses memory.

### Omitted Block Footers

Block footers are omitted to save space and improve overall space efficiency.

## Usage

To use the allocator, include the provided header file and link with the compiled library:

```c
#include "sfmm.h"

int main() {
    // Your code using the allocator
    // ...
    
    return 0;
}
```

An example main.c file is included in the src folder
