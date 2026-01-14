/*
 * kernel_code.c - Project 2, CMPSC 473
 * Copyright 2023 Ruslan Nikolaev <rnikola@psu.edu>
 * Distribution, modification, or usage without explicit author's permission
 * is not allowed.
 */

#include <kernel.h>
#include <types.h>
#include <printf.h>
#include <malloc.h>
#include <string.h>

//PTE Struct:
typedef unsigned long long ull;

struct pte{
  ull p:1;
  ull rw:1;
  ull us:1;
  ull pwt:1;
  ull pcd:1;
  ull a:1;
  ull ign:1;
  ull mbz1:1;
  ull mbz2:1;
  ull avl:3;
  ull page_addr:40;
  ull avail:7;
  ull pke:4;
  ull nonexec:1;
};

void *page_table = NULL; /* TODO: Must be initialized to the page table address */
void *user_stack = NULL; /* TODO: Must be initialized to a user stack virtual address */
void *user_program = NULL; /* TODO: Must be initialized to a user program virtual address */

void kernel_init(void *ustack, void *uprogram, void *memory, size_t memorySize)
{
  //PTE for kernel
  struct pte *p = (struct pte *) memory;
  for (size_t i=0; i < 1048576; i++){
    p[i].p = 1;
    p[i].rw = 1;
    p[i].us = 0;
    p[i].pwt = 0;
    p[i].pcd = 0;
    p[i].a = 0;
    p[i].ign = 0;
    p[i].mbz1 = 0;
    p[i].mbz2 = 0;
    p[i].avl = 0;
    p[i].page_addr = i;
    p[i].avail = 0;
    p[i].pke = 0;
    p[i].nonexec = 0;
  }

  //PDE for kernel
  struct pte *pd = (struct pte *)(p + 1048576);
  for (size_t i = 0; i < 2048; i++){
    struct pte *start = p + 512*i;
    pd[i].p = 1;
    pd[i].rw = 1;
    pd[i].us = 0;
    pd[i].pwt = 0;
    pd[i].pcd = 0;
    pd[i].a = 0;
    pd[i].ign = 0;
    pd[i].mbz1 = 0;
    pd[i].mbz2 = 0;
    pd[i].avl = 0;
    pd[i].page_addr = (ull) start >> 12;
    pd[i].avail = 0;
    pd[i].pke = 0;
    pd[i].nonexec = 0;
  }

  //PDPE for kernel
  struct pte *pdp = (struct pte *) (pd + 2048);
  
  for (size_t i = 0; i < 512; i++){
    struct pte *start2 = pd + 512*i;
    
    if (i < 4){
      pdp[i].p = 1;
      pdp[i].rw = 1;
      pdp[i].us = 0;
      pdp[i].pwt = 0;
      pdp[i].pcd = 0;
      pdp[i].a = 0;
      pdp[i].ign = 0;
      pdp[i].mbz1 = 0;
      pdp[i].mbz2 = 0;
      pdp[i].avl = 0;
      pdp[i].page_addr = (ull) start2 >> 12;
      pdp[i].avail = 0;
      pdp[i].pke = 0;
      pdp[i].nonexec = 0;
    }

    else{
      pdp[i].p = 0;
      pdp[i].rw = 0;
      pdp[i].us = 0;
      pdp[i].pwt = 0;
      pdp[i].pcd = 0;
      pdp[i].a = 0;
      pdp[i].ign = 0;
      pdp[i].mbz1 = 0;
      pdp[i].mbz2 = 0;
      pdp[i].avl = 0;
      pdp[i].page_addr = 0;
      pdp[i].avail = 0;
      pdp[i].pke = 0;
      pdp[i].nonexec = 0;
    }
  }

    //PML4E for kernel/user
  struct pte *pml = (struct pte *) (pdp + 512);

  
  for (size_t i = 0; i < 512; i++){
    struct pte *start3 = pdp + 512*i;
    
    if (i < 1){
      pml[i].p = 1;
      pml[i].rw = 1;
      pml[i].us = 0;
      pml[i].pwt = 0;
      pml[i].pcd = 0;
      pml[i].a = 0;
      pml[i].ign = 0;
      pml[i].mbz1 = 0;
      pml[i].mbz2 = 0;
      pml[i].avl = 0;
      pml[i].page_addr = (ull) start3 >> 12;
      pml[i].avail = 0;
      pml[i].pke = 0;
      pml[i].nonexec = 0;
    }
    
    else{
      pml[i].p = 0;
      pml[i].rw = 0;
      pml[i].us = 0;
      pml[i].pwt = 0;
      pml[i].pcd = 0;
      pml[i].a = 0;
      pml[i].ign = 0;
      pml[i].mbz1 = 0;
      pml[i].mbz2 = 0;
      pml[i].avl = 0;
      pml[i].page_addr = 0;
      pml[i].avail = 0;
      pml[i].pke = 0;
      pml[i].nonexec = 0;
    }
  }

  /* from lec 22: */
  /* create a new page 3, initialize everything to 0 except last entry; same for 2 & 1 and then create mappings */
  
  // PDPE for user
  struct pte *pdp_user = (struct pte *) (pml + 512);// Level 3 for user 
  // PDE for user 
  struct pte *pd_user = (struct pte *) (pdp_user + 512); // Level 2 for user
  // PTE for user
  struct pte *p_user = (struct pte *) (pd_user + 512); // Level 1 for user
  
  // everything in user lists initialized to 0 (user perms are set to 1 later)
  // PDP for user  
  for (int i = 0; i < 512; i++)
    {
      pdp_user[i].p = 0;
      pdp_user[i].rw = 0;
      pdp_user[i].us = 0;
      pdp_user[i].pwt = 0;
      pdp_user[i].pcd = 0;
      pdp_user[i].a = 0;
      pdp_user[i].ign = 0;
      pdp_user[i].mbz1 = 0;
      pdp_user[i].mbz2 = 0;
      pdp_user[i].avl = 0;
      pdp_user[i].page_addr = 0;
      pdp_user[i].avail = 0;
      pdp_user[i].pke = 0;
      pdp_user[i].nonexec = 0;
    }

  // PD for user 
  for (int i = 0; i < 512; i++)
    {
      pd_user[i].p = 0;
      pd_user[i].rw = 0;
      pd_user[i].us = 0;
      pd_user[i].pwt = 0;
      pd_user[i].pcd = 0;
      pd_user[i].a = 0;
      pd_user[i].ign = 0;
      pd_user[i].mbz1 = 0;
      pd_user[i].mbz2 = 0;
      pd_user[i].avl = 0;
      pd_user[i].page_addr = 0;
      pd_user[i].avail = 0;
      pd_user[i].pke = 0;
      pd_user[i].nonexec = 0;
    }

  // PTE for user 
  for (int i = 0; i < 512; i++)
    {
      p_user[i].p = 0;
      p_user[i].rw = 0;
      p_user[i].us = 0;
      p_user[i].pwt = 0;
      p_user[i].pcd = 0;
      p_user[i].a = 0;
      p_user[i].ign = 0;
      p_user[i].mbz1 = 0;
      p_user[i].mbz2 = 0;
      p_user[i].avl = 0;
      p_user[i].page_addr = 0;
      p_user[i].avail = 0;
      p_user[i].pke = 0;
      p_user[i].nonexec = 0;
    }
  
  // setting permissions and mapping page addresses
  
  pml[511].p = 1;
  pml[511].rw = 1;
  pml[511].us = 1; //user space
  //mapping pdp to pml
  pml[511].page_addr = ((ull)pdp_user) >> 12;

  pdp_user[511].p = 1;
  pdp_user[511].rw = 1;
  pdp_user[511].us = 1;
  // mapping PD to PDP
  pdp_user[511].page_addr = ((ull)pd_user) >> 12;

  pd_user[511].p = 1;
  pd_user[511].rw = 1;
  pd_user[511].us = 1;
  // mapping PTEs to PDP
  pd_user[511].page_addr = ((ull)p_user) >> 12;

  // setting last addr of p to uprogram
  p_user[511].p = 1;
  p_user[511].rw = 1;
  p_user[511].us = 1;
  p_user[511].page_addr = ((ull)uprogram) >> 12;

  // setting 2nd to last addr of p to ustack
  p_user[510].p = 1;
  p_user[510].rw = 1;
  p_user[510].us = 1;
  // subtracts 4096 from ustack addr
  p_user[510].page_addr = ((ull)(ustack-4096)) >> 12;

  page_table = pml;

  // user_stack = ustack
  //points to 2nd to last addr (factors in the 4096 addition) 
  user_stack = (void*) (0xFFFFFFFFFFFFFFFF);
  
  // user_program = uprogram;
  // should point to last address
  user_program = (void*)-4096L;
  
	// The remaining portion just loads the page table,
	// this does not need to be changed:
	// load 'page_table' into the CR3 register
  const char *err = load_page_table(page_table);
  if (err != NULL) {
    printf("ERROR: %s\n", err);
  }

  // The extra credit assignment
  mem_extra_test();
}

/* The entry point for all system calls */
long syscall_entry(long n, long a1, long a2, long a3, long a4, long a5)
{
  /*Check if we receive system call n == 1.
  then print the string stored in a1,
  and return 0 as the status.*/
  if (n == 1){
    char* strng = (char*) a1;

    printf("%s", strng);

    return 0;
  }
  //Return -1 for all other cases.
  return -1; /* Success: 0, Failure: -1 */
}
