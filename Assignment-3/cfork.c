#include <cfork.h>
#include <page.h>
#include <mmap.h>



/* You need to implement cfork_copy_mm which will be called from do_cfork in entry.c. Don't remove copy_os_pts()*/
void cfork_copy_mm(struct exec_context *child, struct exec_context *parent ){

    void *os_addr;
   u64 vaddr; 
   struct mm_segment *seg;

   child->pgd = os_pfn_alloc(OS_PT_REG);

   os_addr = osmap(child->pgd);
   bzero((char *)os_addr, PAGE_SIZE);
   
   //CODE segment
   seg = &parent->mms[MM_SEG_CODE];
   for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      if(parent_pte)
           install_ptable((u64) os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
   } 
   //RODATA segment
   
   seg = &parent->mms[MM_SEG_RODATA];
   for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      if(parent_pte)
           install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
   } 
   
   //DATA segment
  seg = &parent->mms[MM_SEG_DATA];
  for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      if(parent_pte){
            u64 pfn = map_physical_page((u64)os_addr, vaddr,MM_RD,(*parent_pte & FLAG_MASK) >> PAGE_SHIFT);
            asm volatile ("invlpg (%0);" 
                                :: "r"(vaddr) 
                                : "memory");
            pfn = map_physical_page((u64)osmap(parent->pgd), vaddr,MM_RD,(*parent_pte & FLAG_MASK) >> PAGE_SHIFT);                     
            asm volatile ("invlpg (%0);" 
                                    :: "r"(vaddr) 
                                    : "memory");
            struct pfn_info* pfn_obj = get_pfn_info(pfn);
            increment_pfn_info_refcount(pfn_obj);
      }
  } 
  
  //STACK segment
  seg = &parent->mms[MM_SEG_STACK];
  for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
     if(parent_pte){
           u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0);  //Returns the blank page  
           pfn = (u64)osmap(pfn);
           memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
      }
  } 
    struct vm_area* vm_area = parent->vm_area;
    while(vm_area){
        u64 vstart = vm_area->vm_start;
        u64 vend = vm_area->vm_end;
        while(vstart < vend){
            u64 *parent_pte =  get_user_pte(parent, vstart, 0);
            if(parent_pte){
                u64 pfn1 = map_physical_page((u64)osmap(parent->pgd), vstart,MM_RD,(*parent_pte & FLAG_MASK) >> PAGE_SHIFT);
                asm volatile ("invlpg (%0);" 
                                    :: "r"(vstart) 
                                    : "memory");
                u64 pfn = map_physical_page((u64)os_addr, vstart,MM_RD,(*parent_pte & FLAG_MASK) >> PAGE_SHIFT);
                asm volatile ("invlpg (%0);" 
                                    :: "r"(vstart) 
                                    : "memory");
                struct pfn_info* pfn_obj = get_pfn_info(pfn1);
                increment_pfn_info_refcount(pfn_obj);
            }
            vstart += PAGE_SIZE;
        }
        vm_area = vm_area->vm_next;
    }



    copy_os_pts(parent->pgd, child->pgd);
    return;
    
}



/* You need to implement cfork_copy_mm which will be called from do_vfork in entry.c.*/
void vfork_copy_mm(struct exec_context *child, struct exec_context *parent ){
    void *os_addr;
    struct mm_segment* seg;
    struct mm_segment* seg_child;
    os_addr = osmap(child->pgd);
    u64 diff = parent->mms[MM_SEG_STACK].end - parent->mms[MM_SEG_STACK].next_free ;

   
    u64 vaddr,new_addr;
    seg = &parent->mms[MM_SEG_STACK];
    for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        if(parent_pte){
            u64 pfn = install_ptable((u64)os_addr, seg, vaddr - diff, 0);  //Returns the blank page  
            pfn = (u64)osmap(pfn);
            memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
            }
    } 

    seg = &child->mms[MM_SEG_STACK];
    seg->start-=diff;
    seg->end-=diff;
    seg->next_free-=diff;
child->regs.rbp -= diff;
    child->regs.entry_rsp -= diff;
    u64 vaddr1 = child->regs.rbp;
        while((vaddr1 < seg->end)&&(vaddr1 >= child->regs.rbp)){
            u64 *v_addr = (u64 *)vaddr1;
            *v_addr -= diff;
            vaddr1 = *v_addr;
        }
    parent->state = WAITING;
    return;
    
}

/*You need to implement handle_cow_fault which will be called from do_page_fault 
incase of a copy-on-write fault

* For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
*/

int handle_cow_fault(struct exec_context *current, u64 cr2){
    // printk("hi1\n");    
    if(cr2 % PAGE_SIZE){
        cr2 -= cr2 % PAGE_SIZE;
    }
    if((cr2 >= MMAP_AREA_START)&&(cr2 < MMAP_AREA_END)){
        // printk("hi3\n"); 
        struct vm_area* vm_area = current->vm_area;
        int present = 0;
        while(vm_area){
            if((cr2 >= vm_area->vm_start) && (cr2 < vm_area->vm_end)){
                present = 1;
                if(vm_area->access_flags&PROT_WRITE){
                    break;
                }
                else{
                    asm volatile ("invlpg (%0);" 
                                            :: "r"(cr2) 
                                            : "memory");
                    return -EINVAL;
                }
            }
            vm_area = vm_area->vm_next;
        }
        if(present == 0){
            return -EINVAL;
        } 
        u64 *parent_pte =  get_user_pte(current, cr2, 0);
        if(parent_pte){
            u64 pfn = (*parent_pte & FLAG_MASK) >> PAGE_SHIFT;
            struct pfn_info* pfn_obj = get_pfn_info(pfn);
            int count = get_pfn_info_refcount(pfn_obj);
            if(count > 1){
                
                u64 pfn = map_physical_page((u64)osmap(current->pgd), cr2,MM_WR|MM_RD,0);
                asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) 
                    : "memory");
                pfn= (u64)osmap(pfn);
                memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE);
                decrement_pfn_info_refcount(pfn_obj);
            }
            else{
                u64 pfn = map_physical_page((u64)osmap(current->pgd), cr2,MM_WR|MM_RD,pfn);
            }
        }
        else{
            // printk("not possible");
            return -EINVAL;
        }
    }
    else if((cr2 >= current->mms[MM_SEG_DATA].start)&&(cr2 < current->mms[MM_SEG_DATA].end)){
        // printk("hi2\n"); 
        if(current->mms[MM_SEG_DATA].access_flags&MM_WR){
            u64 *parent_pte =  get_user_pte(current, cr2, 0);
            if(parent_pte){
                u64 pfn = (*parent_pte & FLAG_MASK) >> PAGE_SHIFT;
                struct pfn_info* pfn_obj = get_pfn_info(pfn);
                int count = get_pfn_info_refcount(pfn_obj);
                if(count > 1){
                    
                    u64 pfn = map_physical_page((u64)osmap(current->pgd), cr2,MM_WR|MM_RD,0);   
                    asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) 
                    : "memory");
                    pfn= (u64)osmap(pfn);
                        memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE);
                    decrement_pfn_info_refcount(pfn_obj);    
                }
                else{
                    u64 pfn = map_physical_page((u64)osmap(current->pgd), cr2,MM_WR|MM_RD,pfn);
                        
                }
            }
            else{
                // printk("not possible");
                return -EINVAL;    
            }

        }
        else{
            asm volatile ("invlpg (%0);" 
                                            :: "r"(cr2) 
                                            : "memory");
            return -EINVAL;
        }
    }
    asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) 
                    : "memory");


    
    return 1;
}

/* You need to handle any specific exit case for vfork here, called from do_exit*/
void vfork_exit_handle(struct exec_context *ctx){
    struct mm_segment *seg;
    struct exec_context * parent = get_ctx_by_pid(ctx->ppid);
    if(parent->state!=WAITING) return; 
    parent->state = READY;

    parent->mms[MM_SEG_DATA] = ctx->mms[MM_SEG_DATA];
    parent->vm_area = ctx->vm_area;

    seg = &ctx->mms[MM_SEG_STACK];
    for(u64 vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
        do_unmap_user(ctx,vaddr);
    }



  return;
}