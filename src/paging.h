#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>
#include "page.h"

/* i386 Page Directory Entry (PDE) */
struct page_directory_entry {
   uint32_t present       : 1;  
   uint32_t rw            : 1;  
   uint32_t user          : 1;  
   uint32_t writethru     : 1;  
   uint32_t cachedisabled : 1;  
   uint32_t accessed      : 1;  
   uint32_t pagesize      : 1;  
   uint32_t ignored       : 2;  
   uint32_t os_specific   : 3;  
   uint32_t frame         : 20; 
};

/* i386 Page Table Entry (PTE) */
struct page {
   uint32_t present    : 1;  
   uint32_t rw         : 1;  
   uint32_t user       : 1;  
   uint32_t accessed   : 1;  
   uint32_t dirty      : 1;  
   uint32_t unused     : 7;  
   uint32_t frame      : 20; 
};

/* Global page directory and a single page table for initial identity mappings */
extern struct page_directory_entry pd[1024];
extern struct page pt0[1024];

void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd_root);
void loadPageDirectory(struct page_directory_entry *pd_root);
void enable_paging(void);
void init_paging_structs(void);

#endif /* PAGING_H */
