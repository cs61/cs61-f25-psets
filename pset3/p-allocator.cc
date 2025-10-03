#include "u-lib.hh"
#ifndef ALLOC_SLOWDOWN
#define ALLOC_SLOWDOWN 100
#endif

extern uint8_t end[];

// These global variables go on the data page.
volatile uintptr_t heap_bottom;
volatile uintptr_t heap_top;
volatile uintptr_t stack_bottom;

// Ensure kernel can load multi-page programs by including some big objects.
struct test_struct {
    int field1;
    unsigned char buf[4096];
    int field2;
};
const test_struct test = {61, {0}, 6161};

inline volatile uint8_t* heap_byte_ptr(uintptr_t addr) {
    assert(addr >= heap_bottom && addr <= heap_top);
    return reinterpret_cast<volatile uint8_t*>(addr);
}

inline volatile long* heap_long_ptr(uintptr_t addr) {
    assert(addr >= heap_bottom && addr <= heap_top && (addr & 7) == 0);
    return reinterpret_cast<volatile long*>(addr);
}

void process_main() {
    assert(test.field1 == 61);
    assert(memchr(&test, 0x11, sizeof(test)) == &test.field2);

    pid_t p = sys_getpid();
    srand(p);

    // The heap starts on the page right after the `end` symbol,
    // whose address is the first address not allocated to process code
    // or data.
    heap_bottom = round_up(reinterpret_cast<uintptr_t>(end), PAGESIZE);

    // The heap is initially empty.
    heap_top = heap_bottom;

    // The bottom of the stack is the first address on the current
    // stack page (this process never needs more than one stack page).
    stack_bottom = round_down(rdrsp() - 1, PAGESIZE);

    // Allocate heap pages until (1) hit the stack (out of address space)
    // or (2) allocation fails (out of physical memory).
    while (heap_top != stack_bottom) {
        if (rand(0, ALLOC_SLOWDOWN - 1) < p) {
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
        }
        sys_yield();
    }

    // After running out of memory, do nothing forever
    while (true) {
        sys_yield();
    }
}
