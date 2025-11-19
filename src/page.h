#ifndef PAGE_H
#define PAGE_H

/* Page Frame Allocator interface.
 * Each physical page is 2MB in size for this assignment.
 * We manage PAGE_COUNT contiguous 2MB frames covering PAGE_COUNT*2MB of physical memory.
 * Allocation returns a doubly-linked list of struct ppage nodes or NULL if insufficient free frames.
 */

#define PAGE_SIZE_MB 2
#define PAGE_COUNT 128
#define PAGE_SIZE_BYTES (PAGE_SIZE_MB * 1024 * 1024)

struct ppage {
    struct ppage *next;
    struct ppage *prev;
    void *physical_addr; /* Starting physical address of this 2MB page */
};

/* Head of the free physical pages doubly linked list */
extern struct ppage *free_physical_pages;

void init_pfa_list(void);
/* Allocate npages frames; on failure returns NULL and preserves free list state. */
struct ppage *allocate_physical_pages(unsigned int npages);
/* Return a list of pages to the free list. The assignment requested the
 * exact function name `free_physical_pages`, so expose that here.
 */
void free_physical_pages(struct ppage *ppage_list);

#endif /* PAGE_H */
