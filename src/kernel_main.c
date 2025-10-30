
#include <stdint.h>
#include "paging.h"
#include "page.h"
#include <stddef.h>

/* Multiboot header omitted for macOS clang build; would normally reside in a dedicated ELF section. */

uint8_t inb (uint16_t _port) {
    (void)_port;
    return 0; /* Stubbed for host build; real implementation requires inline asm with cross-compiler. */
}

extern unsigned int _end_kernel; /* Provided by linker script */

void main() {
    /* Initialize page frame allocator list (not used for identity mapping but for future allocations) */
    init_pfa_list();

    /* Initialize paging structures */
    init_paging_structs();

    /* Identity map kernel from 0x100000 to &_end_kernel */
    uintptr_t kernel_start = 0x100000;
    uintptr_t kernel_end = (uintptr_t)&_end_kernel;
    for (uintptr_t addr = kernel_start; addr < kernel_end; addr += 0x1000) {
        struct ppage tmp; tmp.next = NULL; tmp.prev = NULL; tmp.physical_addr = (void*)addr; /* identity */
        map_pages((void*)addr, &tmp, pd);
    }

    /* Identity map current stack page */
    uint32_t esp;
    asm("mov %%esp,%0" : "=r" (esp));
    uintptr_t stack_page = esp & ~0xFFF; /* page-align */
    struct ppage stmp; stmp.next = NULL; stmp.prev = NULL; stmp.physical_addr = (void*)stack_page;
    map_pages((void*)stack_page, &stmp, pd);

    /* Identity map video buffer (0xB8000) */
    struct ppage vtmp; vtmp.next = NULL; vtmp.prev = NULL; vtmp.physical_addr = (void*)0xB8000;
    map_pages((void*)0xB8000, &vtmp, pd);

    /* Load page directory into CR3 and enable paging */
    loadPageDirectory(pd);
    enable_paging();

    unsigned short *vram = (unsigned short*)0xb8000; // Base address of video mem
    const unsigned char color = 7; // gray text on black background

    while(1) {
        uint8_t status = inb(0x64);

        if(status & 1) {
            uint8_t scancode = inb(0x60);
        }
    }
}
