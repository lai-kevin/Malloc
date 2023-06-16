# Memory-Allocator
A x86-64 dynamic memory allocator written in C. Completed as part of a homework assignment. Utilizes segregated free lists and "quick lists" to improve time complexity. Immediately coalesces large blocks with adjacent free blocks on free, splits without splinters, and omits block footer to improve space efficiency.

How to run:
1. Compile
```
  $ make build
```
2. Run
```
  $ bin/sfmm
```
