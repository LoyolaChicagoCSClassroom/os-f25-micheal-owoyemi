#include <stdint.h>
#include <stddef.h>
#include "paging.h"

/* Aligned global structures */
struct page_directory_entry pd[1024] __attribute__((aligned(4096)));
struct page pt0[1024] __attribute__((aligned(4096)));

static void memset32(uint32_t *dst, uint32_t value, size_t count) {
    for (size_t i = 0; i < count; ++i) dst[i] = value;
}

/* Map a linked list of physical pages beginning at virtual address vaddr.
 * Returns starting virtual address. For simplicity we only use one page table (pt0) and assume
 * virtual addresses are below 4MB for this homework scenario.
 */
void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd_root) {
    uintptr_t v = (uintptr_t)vaddr;
    if (!pd_root || !pglist) return NULL;

    /* Ensure first PDE points to pt0 */
    if (!pd_root[0].present) {
        pd_root[0].present = 1;
        pd_root[0].rw = 1;
        pd_root[0].user = 0;
        pd_root[0].writethru = 0;
        pd_root[0].cachedisabled = 0;
        pd_root[0].accessed = 0;
        pd_root[0].pagesize = 0; /* 0 => 4KB pages */
        pd_root[0].ignored = 0;
        pd_root[0].os_specific = 0;
        pd_root[0].frame = ((uintptr_t)pt0) >> 12; /* physical addr of pt0 */
    }

    struct ppage *curr = pglist;
    while (curr) {
        /* Compute page table index */
        unsigned pti = (v >> 12) & 0x3FF; /* bits 12..21 */
        struct page *pte = &pt0[pti];
        pte->present = 1;
        pte->rw = 1;
        pte->user = 0;
        pte->accessed = 0;
        pte->dirty = 0;
        pte->unused = 0;
        pte->frame = ((uintptr_t)curr->physical_addr) >> 12;
        v += 0x1000; /* advance by 4KB */
        curr = curr->next;
    }
    return vaddr;
}

void loadPageDirectory(struct page_directory_entry *pd_root) {
    asm("mov %0,%%cr3" :: "r"(pd_root));
}

void enable_paging(void) {
    asm("mov %cr0, %eax\n"
        "or $0x80000001,%eax\n" /* set PG and PE bits */
        "mov %eax,%cr0");
}

/* Simple initialization for pd/pt0 (clear entries). */
void init_paging_structs(void) {
    memset32((uint32_t*)pd, 0, 1024);
    memset32((uint32_t*)pt0, 0, 1024);
}
