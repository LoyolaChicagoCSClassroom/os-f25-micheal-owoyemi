#include <stddef.h>
#include <stdint.h>
#include "page.h"

/* Statically allocated array representing all physical pages the kernel can manage. */
struct ppage physical_page_array[PAGE_COUNT];

/* Head pointer for free list of physical pages. */
struct ppage *free_physical_pages = NULL;

static inline void list_append(struct ppage **head, struct ppage *node) {
    if (!node) return;
    node->next = NULL;
    if (!*head) {
        node->prev = NULL;
        *head = node;
        return;
    }
    struct ppage *tail = *head;
    while (tail->next) tail = tail->next;
    tail->next = node;
    node->prev = tail;
}

static inline struct ppage *list_pop_front(struct ppage **head) {
    if (!head || !*head) return NULL;
    struct ppage *front = *head;
    *head = front->next;
    if (*head) (*head)->prev = NULL;
    front->next = front->prev = NULL;
    return front;
}

/* Initialize the page frame allocator free list.
 * Assumes physical memory start at 0x0 for simplicity; real kernels would
 * discover available RAM via firmware/bootloader data (e.g., multiboot memory map).
 */
void init_pfa_list(void) {
    free_physical_pages = NULL;
    /* Assume a contiguous physical memory region starting at 0 for simplicity. */
    uintptr_t base = 0x0;
    for (unsigned int i = 0; i < PAGE_COUNT; ++i) {
        struct ppage *pp = &physical_page_array[i];
        pp->physical_addr = (void *)(base + (i * PAGE_SIZE_BYTES));
        pp->next = pp->prev = NULL;
        list_append(&free_physical_pages, pp);
    }
}

/* Allocate npages from free list; returns head of a new list of allocated pages or NULL if insufficient pages.
 * Failure path rolls back any partially allocated frames to free list to keep allocator consistent.
 */
struct ppage *allocate_physical_pages(unsigned int npages) {
    if (npages == 0) return NULL;
    struct ppage *alloc_head = NULL;
    struct ppage *alloc_tail = NULL;

    for (unsigned int i = 0; i < npages; ++i) {
        struct ppage *pp = list_pop_front(&free_physical_pages);
        if (!pp) {
            /* Not enough pages; roll back already allocated pages back to free list. */
            if (alloc_head) {
                free_physical_pages_list(alloc_head);
            }
            return NULL;
        }
        /* Append to allocated list */
        pp->next = pp->prev = NULL;
        if (!alloc_head) {
            alloc_head = alloc_tail = pp;
        } else {
            alloc_tail->next = pp;
            pp->prev = alloc_tail;
            alloc_tail = pp;
        }
    }
    return alloc_head;
}

/* Return a list of pages to the free list. */
void free_physical_pages_list(struct ppage *ppage_list) {
    if (!ppage_list) return;
    /* Walk to head of incoming list */
    while (ppage_list->prev) ppage_list = ppage_list->prev;
    struct ppage *curr = ppage_list;
    while (curr) {
        struct ppage *next = curr->next;
        curr->next = curr->prev = NULL;
        list_append(&free_physical_pages, curr);
        curr = next;
    }
}

/* Public API matching the homework specification. */
void free_physical_pages(struct ppage *ppage_list) {
    free_physical_pages_list(ppage_list);
}
