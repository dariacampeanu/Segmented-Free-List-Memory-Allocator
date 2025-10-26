# Segmented Free List Memory Allocator

This project implements a custom memory management system in C, designed to simulate the behavior of an operating system heap using the Segmented Free List (SFL) model. It provides efficient memory allocation, deallocation, and fragmentation control through linked data structures.

## Overview

The allocator manages memory through multiple linked lists, where each list contains blocks of a specific size. It supports commands equivalent to `malloc`, `free`, `read`, and `write`, while maintaining information about allocated and free memory blocks. The program includes full error handling, dynamic memory inspection, and cleanup functionality.

## Core Functionality

### Initialization
`INIT_HEAP` creates the main segmented structure, initializing several doubly linked lists that manage memory blocks of increasing sizes (e.g., 8B, 16B, 32B). Each list is pre-populated with free nodes that simulate available memory.

### Allocation
When a `MALLOC` command is issued, the allocator searches for the smallest block capable of storing the requested number of bytes. If no exact match exists, it splits a larger block and tracks fragmentation. Allocated blocks are added to a dedicated list for monitoring and debugging.

### Deallocation
`FREE` returns a memory block to the corresponding list based on its size. If a matching list does not exist, the allocator dynamically creates a new one. Invalid frees are safely detected and reported.

### Memory Access
- `WRITE`: Stores a string into the allocated memory region, supporting multi-block writes for contiguous memory segments.  
- `READ`: Outputs stored data from memory, validating the requested address and size.

### Memory Dump
`DUMP_MEMORY` provides a detailed overview of the heap state, showing total, allocated, and free memory, number of allocations, frees, and fragmentations. It lists the addresses of free and allocated blocks for debugging and transparency.

### Cleanup
`DESTROY_HEAP` deallocates all memory structures, restoring the system to a clean state.

## Data Structures

- **seg_free_list_t** – Represents the segmented free list allocator.  
- **doubly_linked_list_t** – Manages memory blocks of uniform size.  
- **dll_node_t** – Node element containing address, size, and block data.  
- **info_t** – Metadata for each block, including address, size, and stored string.
