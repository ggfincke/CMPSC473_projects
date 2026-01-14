# CMPSC 473 Operating Systems Projects

Three projects covering core OS concepts: dynamic memory allocation, virtual memory/paging, and concurrent channel-based communication.

**Course:** CMPSC 473 - Operating Systems
**Institution:** Penn State
**Term:** Fall 2023
**Team:** Garrett Fincke & Avanish Grampurohit

---

## Projects Overview

| Project | Topic | Key Concepts |
|---------|-------|--------------|
| 1 | Memory Allocator | Segregated free lists, coalescing, malloc/free |
| 2 | Page Tables | x86-64 paging, virtual memory, kernel/user space |
| 3 | Channels | Semaphores, mutexes, blocking I/O, select() |

---

## Project 1: Dynamic Memory Allocator

Custom `malloc`, `free`, and `realloc` implementation using segregated free lists.

**Design:**
- 61 segregated free lists organized by block size (power-of-2 classes)
- Explicit free list with LIFO insertion
- Boundary tags (header/footer) for coalescing
- 16-byte alignment

**Key Functions:**
| Function | Description |
|----------|-------------|
| `mm_malloc()` | Find fit in segregated list, split if needed |
| `mm_free()` | Add to free list, coalesce adjacent blocks |
| `mm_realloc()` | Resize block, copy data if necessary |
| `coalesce()` | Merge adjacent free blocks (4 cases) |
| `find_fit()` | Best-fit search across size classes |

**Block Structure:**
```
┌────────┬──────────────────────────┬────────┐
│ Header │        Payload           │ Footer │
│ 8 bytes│    (aligned to 16)       │ 8 bytes│
└────────┴──────────────────────────┴────────┘
```

---

## Project 2: Page Table Implementation

x86-64 four-level page table setup for kernel and user space virtual memory.

**Page Table Hierarchy:**
```
PML4 (Level 4)
  └─> PDPE (Level 3)
        └─> PDE (Level 2)
              └─> PTE (Level 1)
                    └─> Physical Page
```

**Implementation:**
- Maps 4GB kernel space (identity mapped)
- Creates user space mappings for program and stack
- Configures permission bits (present, read/write, user/supervisor)

**PTE Structure (64-bit):**
| Bits | Field | Purpose |
|------|-------|---------|
| 0 | P | Present |
| 1 | R/W | Read/Write |
| 2 | U/S | User/Supervisor |
| 12-51 | Page Addr | Physical page frame number |
| 63 | NX | No-Execute |

Also implements a basic `syscall_entry()` handler for user-kernel transitions.

---

## Project 3: Concurrent Channels

Thread-safe message passing channels (similar to Go channels) with blocking and non-blocking operations.

**Channel API:**
| Function | Description |
|----------|-------------|
| `channel_create()` | Allocate channel with bounded buffer |
| `channel_send()` | Send message (blocking or non-blocking) |
| `channel_receive()` | Receive message (blocking or non-blocking) |
| `channel_close()` | Close channel, wake blocked threads |
| `channel_destroy()` | Free channel resources |
| `channel_select()` | Multiplex across multiple channels |

**Synchronization:**
- Semaphores for buffer space tracking (`available`, `used`)
- Mutex for critical section protection
- Linked lists for tracking waiting threads in `select()`

**Return Codes:**
- `SUCCESS` - Operation completed
- `WOULDBLOCK` - Non-blocking call on full/empty channel
- `CLOSED_ERROR` - Channel was closed
- `DESTROY_ERROR` - Destroy called on open channel

---

## Building

Each project has its own Makefile:

```bash
cd p[X]-*/p[X]-temp/p[X]-*/
make
```

## Project Structure

```
CMPSC473_projects/
├── p1-memory-management/
│   └── p1-temp/p1-memory-management/
│       ├── mm.c              # Allocator implementation
│       ├── memlib.c          # Heap simulation
│       ├── mdriver.c         # Test driver
│       └── traces/           # Test workloads
├── p2-thread-scheduler/
│   └── p2-temp/p2-thread-scheduler/
│       └── kernel/
│           ├── kernel_code.c # Page table setup
│           ├── kernel.c      # Kernel main
│           └── kernel_asm.S  # Assembly routines
└── p3-system-call-interface/
    └── p3-temp/p3-system-call-interface/
        ├── channel.c         # Channel implementation
        ├── buffer.c          # Bounded buffer
        ├── linked_list.c     # List for select()
        └── stress.c          # Stress tests
```
