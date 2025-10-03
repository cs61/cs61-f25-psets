#include "u-lib.hh"
#ifndef ALLOC_SLOWDOWN
#define ALLOC_SLOWDOWN 100
#endif

extern uint8_t end[];

uintptr_t heap_bottom;
uintptr_t heap_top;
uintptr_t stack_bottom;

// Ensure kernel can load multi-page programs by including some big objects.
struct test_struct {
    int field1;
    unsigned char buf[4096];
    int field2;
};
const test_struct test1 = {61, {0}, 6161};
volatile test_struct test2;

inline volatile uint8_t* heap_byte_ptr(uintptr_t addr) {
    assert(addr >= heap_bottom && addr <= heap_top);
    return reinterpret_cast<volatile uint8_t*>(addr);
}

inline volatile long* heap_long_ptr(uintptr_t addr) {
    assert(addr >= heap_bottom && addr <= heap_top && (addr & 7) == 0);
    return reinterpret_cast<volatile long*>(addr);
}

void process_main() {
    pid_t initial_pid = sys_getpid();
    assert(initial_pid > 0 && initial_pid < MAXNPROC);
    test2.field1 = 61;
    assert(test1.field1 == 61 && test1.field2 == 6161);

    // Fork a total of three new copies, checking `fork` return values.
    pid_t p1 = sys_fork();
    assert(p1 >= 0 && p1 < MAXNPROC);
    pid_t intermediate_pid = sys_getpid();
    if (p1 == 0) {
        assert(intermediate_pid != initial_pid);
    } else {
        assert(intermediate_pid == initial_pid && p1 != initial_pid);
    }

    pid_t p2 = sys_fork();
    assert(p2 >= 0 && p2 < MAXNPROC);
    pid_t final_pid = sys_getpid();
    if (p2 == 0) {
        assert(final_pid != initial_pid && final_pid != intermediate_pid);
    } else {
        assert(p2 != p1 && p2 != intermediate_pid && p2 != initial_pid);
        assert(final_pid == intermediate_pid);
    }

    // Check that multi-page segments can be loaded.
    assert(test1.field1 == 61 && test1.field2 == 6161);
    assert(test2.field1 == 61);
    test2.field2 = 61 + final_pid;
    sys_yield();
    assert(test2.field2 == 61 + final_pid);

    // The rest of this code is like p-allocator.c.

    pid_t p = sys_getpid();
    srand(p);

    heap_bottom = round_up(reinterpret_cast<uintptr_t>(end), PAGESIZE);
    heap_top = heap_bottom;
    stack_bottom = round_down(rdrsp() - 1, PAGESIZE);

    while (heap_top != stack_bottom) {
        int x = rand(0, ALLOC_SLOWDOWN - 1);
        if (x < p) {
            if (sys_page_alloc(reinterpret_cast<void*>(heap_top)) < 0) {
                break;
            }
            auto new_page = heap_top;
            heap_top += PAGESIZE;
            // check that the page starts out all zero
            for (auto l = heap_long_ptr(new_page);
                 l != heap_long_ptr(new_page + PAGESIZE);
                 ++l) {
                assert(*l == 0);
            }
            // check we can write to new page
            *heap_byte_ptr(new_page) = p;
            // check we can write to console
            console[CPOS(24, 79)] = p;
        } else if (x < p + 1 && heap_bottom < heap_top) {
            // ensure we can write to any previously-allocated page
            uintptr_t addr = rand(heap_bottom, heap_top - 1);
            *heap_byte_ptr(addr) = p;
        }
        sys_yield();
    }

    // After running out of memory, do nothing forever
    while (true) {
        sys_yield();
    }
}
