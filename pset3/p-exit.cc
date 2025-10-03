#include "u-lib.hh"
#ifndef ALLOC_SLOWDOWN
#define ALLOC_SLOWDOWN 18
#endif

extern uint8_t end[];

uintptr_t heap_bottom;
uintptr_t heap_top;
uintptr_t stack_bottom;

// Remember which pages we wrote data into
volatile uint8_t pagemark[4096] = {0};

inline volatile uint8_t* heap_byte_ptr(uintptr_t addr) {
    assert(addr >= heap_bottom && addr <= heap_top);
    return reinterpret_cast<volatile uint8_t*>(addr);
}

inline volatile long* heap_long_ptr(uintptr_t addr) {
    assert(addr >= heap_bottom && addr <= heap_top && (addr & 7) == 0);
    return reinterpret_cast<volatile long*>(addr);
}

void process_main() {
    for (size_t i = 0; i != sizeof(pagemark); ++i) {
        assert(pagemark[i] == 0);
    }

    while (true) {
        int x = rand(0, ALLOC_SLOWDOWN);
        if (x == 0) {
            // fork, then either exit or start allocating
            pid_t p = sys_fork();
            assert(p < MAXNPROC);
            int choice = rand(0, 2);
            if (choice == 0 && p > 0) {
                sys_exit();
            } else if (choice != 2 ? p > 0 : p == 0) {
                break;
            }
        } else {
            sys_yield();
        }
    }

    int speed = rand(1, 16);
    pid_t self = sys_getpid();

    heap_bottom = round_up(reinterpret_cast<uintptr_t>(end), PAGESIZE);
    heap_top = heap_bottom;
    stack_bottom = round_down(rdrsp() - 1, PAGESIZE);

    unsigned nalloc = 0;

    // Allocate heap pages until out of address space,
    // forking along the way.
    while (heap_top != stack_bottom) {
        int x = rand(0, 6 * ALLOC_SLOWDOWN);
        if (x >= 8 * speed) {
            if (x % 4 < 2 && heap_top != heap_bottom) {
                unsigned pn = rand(0, (heap_top - heap_bottom - 1) / PAGESIZE);
                if (pn < sizeof(pagemark)) {
                    auto ptr = heap_byte_ptr(heap_bottom + pn * PAGESIZE);
                    assert(*ptr == pagemark[pn]);
                    pagemark[pn] = self;
                    *ptr = self;
                    assert(*ptr == self);
                }
            }
            sys_yield();
            continue;
        }

        x = rand(0, 7 + min(nalloc / 4, 10U));
        if (x < 2) {
            pid_t p = sys_fork();
            assert(p < MAXNPROC);
            if (p == 0) {
                pid_t new_self = sys_getpid();
                assert(new_self != self);
                self = new_self;
                speed = rand(1, 16);
            }
        } else if (x < 3) {
            sys_exit();
        } else if (sys_page_alloc(reinterpret_cast<void*>(heap_top)) >= 0) {
            auto new_page = heap_top;
            heap_top += PAGESIZE;
            nalloc = (heap_top - heap_bottom) / PAGESIZE;
            // check that the page starts out all zero
            for (auto l = heap_long_ptr(new_page);
                l != heap_long_ptr(new_page + PAGESIZE);
                ++l) {
                assert(*l == 0);
            }
            // check we can write to new page
            *heap_byte_ptr(new_page) = speed;
            // check we can write to console
            console[CPOS(24, 79)] = speed;
            // record data written
            unsigned pn = (new_page - heap_bottom) / PAGESIZE;
            if (pn < sizeof(pagemark)) {
                pagemark[pn] = speed;
            }
            // clear "Out of physical memory" msg
            if (console[CPOS(24, 0)]) {
                console_printf(CPOS(24, 0), "\n");
            }
        } else if (nalloc < 4) {
            sys_exit();
        } else {
            nalloc -= 4;
        }
    }

    // After running out of memory
    while (true) {
        if (rand(0, 2 * ALLOC_SLOWDOWN - 1) == 0) {
            sys_exit();
        } else {
            sys_yield();
        }
    }
}
