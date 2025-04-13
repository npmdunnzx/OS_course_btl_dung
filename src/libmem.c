/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
// TODO: commit the vmaid (NOTE: oldddddddd module, ignore it)

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  /* Handle the region management when get_free_vmrg_area fails */
  /* TODO Retrieve current vma */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  /* Calculate increase size aligned to page size */
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  
  /* TODO Record old sbrk before increasing */
  int old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT using systemcall sys_memap with SYSMEM_INC_OP */
  struct sc_regs regs;
regs.a1 = SYSMEM_INC_OP;
regs.a2 = vmaid;
regs.a3 = inc_sz;
  
  /* Invoke syscall 17 (sys_memmap) */
  int ret = syscall(caller, 17, &regs);
  
  if (ret < 0) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  
  /* TODO Commit the allocation address at the old_sbrk */
  *alloc_addr = old_sbrk;
  
  /* Update symbol table for this region */
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}







/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct *rgnode;

  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  /* TODO Manage the collect freed region to freerg_list */
  rgnode = malloc(sizeof(struct vm_rg_struct));
  if (rgnode == NULL) return -1;
  
  /* Copy information from symbol region table */
  rgnode->rg_start = caller->mm->symrgtbl[rgid].rg_start;
  rgnode->rg_end = caller->mm->symrgtbl[rgid].rg_end;
  rgnode->rg_next = NULL;

  /* Reset symbol table entry */
  caller->mm->symrgtbl[rgid].rg_start = 0;
  caller->mm->symrgtbl[rgid].rg_end = 0;

  /* Enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, rgnode);

  return 0;
}





/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  /* TODO Implement allocation on vm area 0 */
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  /* TODO Implement free region */

  /* By default using vmaid = 0 */
  return __free(proc, 0, reg_index);
}





/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller){
uint32_t pte = mm->pgd[pgn];

if (!PAGING_PAGE_PRESENT(pte)){ /* Page is not online, make it actively living */
int vicpgn;
int swpfpn; 
int vicfpn;
uint32_t vicpte;

int tgtfpn = PAGING_PTE_SWP(pte); // The target frame storing our variable

// TODO playing with paging theory
/* Find victim page */
find_victim_page(caller->mm, &vicpgn);
    
    /* Get victim's page table entry */
vicpte = mm->pgd[vicpgn];
vicfpn = PAGING_FPN(vicpte);

/* Get free frame in MEMSWP */
MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

/* TODO Copy victim frame to swap: SWP(vicfpn <--> swpfpn) */
struct sc_regs regs;
regs.a1 = SYSMEM_SWP_OP;
regs.a2 = vicfpn;
regs.a3 = swpfpn;

/* SYSCALL 17 sys_memmap to swap */
syscall(caller, 17, &regs);

/* Update page table entry for victim - set as swapped */
pte_set_swap(&mm->pgd[vicpgn], caller->active_mswp_id, swpfpn);

/* TODO Copy target frame from swap to mem: SWP(tgtfpn <--> vicfpn) */
regs.a1 = SYSMEM_SWP_OP;
regs.a2 = tgtfpn;
regs.a3 = vicfpn;

/* SYSCALL 17 sys_memmap */
syscall(caller, 17, &regs);

/* Update page table entry for target page - mark as present */
pte_set_fpn(&mm->pgd[pgn], vicfpn);
PAGING_PTE_SET_PRESENT(mm->pgd[pgn]);

/* Add to FIFO page queue for future replacement */
enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
}

*fpn = PAGING_FPN(mm->pgd[pgn]);

return 0;
}




// TODO here
/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller){
int pgn = PAGING_PGN(addr);
int off = PAGING_OFFST(addr);
int fpn;

/* Get the page to MEMRAM, swap from MEMSWAP if needed */
if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1; /* invalid page access */

/* Calculate physical address */
int phyaddr = (fpn << (PAGING_ADDR_OFFST_HIBIT + 1)) | off;
  
/* MEMPHY READ using SYSCALL 17 sys_memmap with SYSMEM_IO_READ */
struct sc_regs regs;
regs.a1 = SYSMEM_IO_READ;
regs.a2 = phyaddr;

syscall(caller, 17, &regs);
/* Data is updated via pointer */
*data = (BYTE)regs.a3;
return 0;
}




// TODO here
/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller){
int pgn = PAGING_PGN(addr);
int off = PAGING_OFFST(addr);
int fpn;

/* Get the page to MEMRAM, swap from MEMSWAP if needed */
if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1; /* invalid page access */

/* Calculate physical address */
int phyaddr = (fpn << (PAGING_ADDR_OFFST_HIBIT + 1)) | off;

/* MEMPHY WRITE using SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE */
struct sc_regs regs;
regs.a1 = SYSMEM_IO_WRITE;
regs.a2 = phyaddr;
regs.a3 = value;

syscall(caller, 17, &regs);
  
return 0;
}




/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
*destination = (uint32_t)data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}




/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}




/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, 0, destination, offset, data);
}




/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}




/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn){
struct pgn_t *pg = mm->fifo_pgn;

/* TODO: Implement the theorical mechanism to find the victim page */

// fifo so get first, second move to first
if (pg != NULL){
*retpgn = pg->pgn;
mm->fifo_pgn = pg->pg_next;
}
//If no page in queue, use a simple approach - page 0
else *retpgn = 0;

free(pg);
return 0;
}




// NOTE: this is in mm.h
/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg){
if(caller == NULL || newrg == NULL) return -1;

struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

if (rgit == NULL) return -1;

/* Probe unintialized newrg */
// mean reset this mem region pointer for use
newrg->rg_end = -1;
newrg->rg_start = -1;

/* TODO Traverse on list of free vm region to find a fit space */

for(;rgit != NULL;){
// found fit region
if(rgit->rg_end - rgit->rg_start >= size){
newrg->rg_start = rgit->rg_start;
newrg->rg_end = rgit->rg_start + size;

// Update the used region
//if not used all free space
if (rgit->rg_end - newrg->rg_end > 0){
rgit->rg_start = newrg->rg_end;
}
// if used all free space, remove this rg from free list
else{
cur_vma->vm_freerg_list = rgit->rg_next;
free(rgit);
}
      
return 0;
}

rgit = rgit->rg_next;
}

return -1;
}

//#endif
