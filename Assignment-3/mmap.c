#include<types.h>
#include<mmap.h>


void unmap(struct exec_context *ctx, u64 addr , int length){
    u64 endaddr = addr + length;
    while(addr < endaddr){
        do_unmap_user(ctx,addr);
        addr += PAGE_SIZE;
    }
}

void update_map(unsigned long base, u64 addr , int access_flags , int length, struct exec_context* current){
    u32 permission = 0;
    u64 finaladdr = addr + length;
        if(access_flags&PROT_WRITE){
            permission = (u32) PROT_READ|PROT_WRITE;
        }
        if(access_flags==PROT_READ){
            permission = (u32) PROT_READ;
        }


        while(addr < finaladdr){ 
            u64* pte_addr = get_user_pte(current,addr,0);
            if(pte_addr){
                u32 upfn = (*pte_addr >> 12); 
                upfn = map_physical_page((u64)osmap(base),addr,permission,(u32)0);
                asm volatile ("invlpg (%0);" 
                                :: "r"(addr) 
                                : "memory");
            }
            addr += PAGE_SIZE;                
        } 
}
void map(unsigned long base, u64 addr , int access_flags , int length){  
    u32 permission = 0;
    u64 finaladdr = addr + length;
        if(access_flags&PROT_WRITE){
            permission = (u32) PROT_READ|PROT_WRITE;
        }
        if(access_flags==PROT_READ){
            permission = (u32) PROT_READ;
        }


        while(addr < finaladdr){ 
            u32 upfn = map_physical_page((u64)osmap(base),addr,permission,(u32)0);
            asm volatile ("invlpg (%0);" 
                            :: "r"(addr) 
                            : "memory");
            addr += PAGE_SIZE;                
        }    
}

/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
 */
int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{

    struct vm_area* vm_area = current->vm_area;
    int correct = 0;
    if((error_code == 4)||(error_code == 6) ){
        while(vm_area){
            if((addr >= vm_area->vm_start) && (addr < vm_area->vm_end)){
                if((error_code == 6) && ((vm_area->access_flags&PROT_WRITE)) != 0){
                    correct = 1;
                }
                if((error_code == 4)){
                    correct = 1;
                }
                break;
            }
            vm_area = vm_area->vm_next;
        } 
        if(correct == 0){
            asm volatile ("invlpg (%0);" 
                        :: "r"(addr) 
                        : "memory"); 
            return -EINVAL;
        }
        u32 permission = 0;
        if(vm_area->access_flags&PROT_WRITE){
            permission = (u32) PROT_READ|PROT_WRITE;
        }
        if(vm_area->access_flags==PROT_READ){
            permission = (u32) PROT_READ;
        }
         
        u32 upfn = map_physical_page((u64)osmap(current->pgd),addr,permission,(u32)0);
        asm volatile ("invlpg (%0);" 
                        :: "r"(addr) 
                        : "memory");   // Flush TLB
        return 1;
    }
    asm volatile ("invlpg (%0);" 
                        :: "r"(addr) 
                        : "memory"); 

    int fault_fixed = -1;
    
    return fault_fixed;
}

/**
 * mprotect System call Implementation.
 */
int vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    struct vm_area* t;
    int count3;
    if(length % PAGE_SIZE != 0){
        length += (PAGE_SIZE-(length%PAGE_SIZE));
    }
    struct vm_area* vm_area = current->vm_area;
    struct vm_area vm_area_p;
    vm_area_p.vm_end = MMAP_AREA_START;
    while(vm_area){
        if((addr < vm_area->vm_end) && (addr >= vm_area_p.vm_end) ){
            break;
        }
        vm_area_p = *vm_area;
        vm_area = vm_area->vm_next;
    }
    long addr1 = addr;
    long end = addr + length;
    while(vm_area != NULL){
        if(addr1 >= vm_area->vm_start){
            if(end >= vm_area->vm_end){
                addr1 = vm_area->vm_end;
                vm_area = vm_area->vm_next;
            }
            else{
                addr1 = end;
                break;
            }
        }
        else{
            break;
        }
    }
    if(addr1 != end){
        return -EINVAL;
    }
    int count = 0;
    struct vm_area* root;
    struct vm_area* head = current->vm_area;
    if(head){
        struct vm_area* head2 = alloc_vm_area();
        root = head2;
        while( head){
            // count ++; 
            *head2 = *head;
            if(head->vm_next) head2->vm_next = alloc_vm_area();
            head = head->vm_next;
            head2 = head2->vm_next;
        }

    }
    vm_area = current->vm_area;
    vm_area_p.vm_end = MMAP_AREA_START;
    while(vm_area){
        if((addr < vm_area->vm_end) && (addr >= vm_area_p.vm_end) ){
            break;
        }
        vm_area_p = *vm_area;
        vm_area = vm_area->vm_next;
    }
    while(vm_area){
        if((vm_area->vm_start < addr) && (vm_area->vm_end > addr + length )){
            if(prot == vm_area->access_flags){
                vm_area = vm_area->vm_next;
                break;
            }
            count3=0;
            t = current->vm_area;
            while(t){
                count3++;
                t=t->vm_next;
            }
            if(count3 >= 127){
                struct vm_area* temp;
                while(root){
                    temp = root->vm_next;
                    dealloc_vm_area(root);
                    root = temp;
                }
                return -EINVAL;
            }
            struct vm_area* vm_area_new = alloc_vm_area();
            struct vm_area* vm_area_new2 = alloc_vm_area();
            vm_area_new->vm_start = addr;
            vm_area_new->vm_end = addr + length;
            vm_area_new->access_flags = prot;

            vm_area_new2->vm_start = addr + length ;
            vm_area_new2->vm_end = vm_area->vm_end;
            vm_area_new2->access_flags = vm_area->access_flags;

            vm_area->vm_end = addr;

            vm_area_new->vm_next = vm_area_new2;
            vm_area_new2->vm_next  = vm_area->vm_next;
            vm_area->vm_next = vm_area_new; 
            break;
        }
        if(vm_area->vm_start < addr){
            if(prot == vm_area->access_flags){
                vm_area = vm_area->vm_next;
                continue;
            }
            struct vm_area* vm_area_new = alloc_vm_area();
            vm_area_new->vm_start = addr;
            vm_area_new->vm_end = vm_area->vm_end ;
            vm_area_new->access_flags = prot;

            vm_area->vm_end = addr;

            vm_area_new->vm_next = vm_area->vm_next;
            vm_area->vm_next = vm_area_new;

            vm_area = vm_area_new->vm_next;
            continue;
        }
        else if((vm_area->vm_end <=  addr + length)){
            if(prot == vm_area->access_flags){
                vm_area = vm_area->vm_next;
                continue;
            }
            
            vm_area->access_flags = prot;
            vm_area = vm_area->vm_next;
            continue;
        }
        else if(vm_area->vm_start <=  addr + length ){
            if(prot == vm_area->access_flags){
                vm_area = vm_area->vm_next;
                break;
            }

            struct vm_area* vm_area_new = alloc_vm_area();
            vm_area_new->vm_start = addr+length;
            vm_area_new->vm_end = vm_area->vm_end;
            vm_area_new->access_flags = vm_area->access_flags;
            vm_area->vm_end = addr+length;
            vm_area->access_flags = prot;


            vm_area_new->vm_next = vm_area->vm_next;
            vm_area->vm_next = vm_area_new;
            break;
        }
        break;
    }
    vm_area = current->vm_area;
    struct vm_area* vm_area_prev = current->vm_area;
    if((vm_area)){
        vm_area = vm_area->vm_next;
    }
    while(vm_area){
        if((vm_area->access_flags == vm_area_prev->access_flags)&&(vm_area_prev->vm_end == vm_area->vm_start)){
                vm_area_prev->vm_next = vm_area->vm_next;
                vm_area_prev->vm_end = vm_area->vm_end;
                dealloc_vm_area(vm_area);
                vm_area = vm_area_prev->vm_next;
        }
        else
        {
            vm_area_prev = vm_area;
            vm_area = vm_area->vm_next;
        }
    }
    int num = 0;
    vm_area = current->vm_area;
    while (vm_area)
    {
        num ++;
        vm_area = vm_area->vm_next;
    }
 
    if(num > 128){
        struct vm_area* temp;
        temp = current->vm_area;
        current->vm_area = root;
        root = temp;
        while(root){
            temp = root->vm_next;
            dealloc_vm_area(root);
            root = temp;
        }   
        return -EINVAL;
    }
    else{
       struct vm_area* temp;
        while(root){
        // count--;
        temp = root->vm_next;
        dealloc_vm_area(root);
        root = temp;
        } 
    }
    update_map(current->pgd,addr,prot,length,current);
    return 0;
    int isValid = -1;
    return isValid;
}
/**
 * mmap system call implementation.
 */

long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    int num = 0;
    struct vm_area* vm_area1 = current->vm_area;
    while (vm_area1)
    {
        num ++;
        vm_area1 = vm_area1->vm_next;
    }
    
    long ret_addr = -1;
    // printk("addr = %x\n",addr);
    int found = 0;
    int err = 0;
    if(length % PAGE_SIZE != 0){
        length += (PAGE_SIZE-(length%PAGE_SIZE));
    }
    int pages = length / PAGE_SIZE;
    if((addr >= MMAP_AREA_START) && (addr < MMAP_AREA_END) && (addr + length <= MMAP_AREA_END)){
        struct vm_area* vm_area = current->vm_area;
        struct vm_area* vm_area_prev = current->vm_area;

        while((vm_area != NULL)){
            if((addr + length > vm_area->vm_start) && (addr < vm_area->vm_end)){
                err = 1;
                break;
            }
            vm_area = vm_area->vm_next;
        }
        vm_area = current->vm_area;

        if((vm_area == NULL)){
            if(num >= 128){
                return -EINVAL;
            }
            vm_area = alloc_vm_area();
            vm_area->vm_start = addr;
            vm_area->vm_end = addr + length;
            vm_area->access_flags = prot;
            vm_area->vm_next = NULL;
            
            ret_addr = vm_area->vm_start;
            current->vm_area = vm_area;
            found = 1;

            int res = 1;
            if(flags&MAP_POPULATE){
                map(current->pgd, ret_addr ,prot , length );  
            }          

            return ret_addr;
        }
        else if(err == 0){
            struct vm_area* vm_area_new = alloc_vm_area();
            vm_area_new->vm_start = addr;
            vm_area_new->vm_end = addr + length;
            vm_area_new->access_flags = prot;
            ret_addr = addr;
            found = 1;
            
            vm_area_prev = current->vm_area;
            vm_area = vm_area_prev;
            if(addr < vm_area->vm_start){
                current->vm_area = vm_area_new;
                vm_area_new->vm_next = vm_area;
            }
            else{
                vm_area = vm_area_prev->vm_next;
                while(1){
                    if((addr > vm_area_prev->vm_start) && ((vm_area == NULL)||(addr < vm_area->vm_start))){
                        vm_area_new->vm_next = vm_area;
                        vm_area_prev->vm_next = vm_area_new;
                        break;
                    }
                    if(vm_area == NULL)
                        break;
                    vm_area_prev = vm_area;    
                    vm_area = vm_area->vm_next;
                        
                }
            }
            if(found == 1){
                vm_area_prev = current->vm_area;
                vm_area = vm_area_prev->vm_next;
                while(vm_area){
                    if((vm_area_prev->vm_end == vm_area->vm_start) && (vm_area->access_flags == vm_area_prev->access_flags)){
                        vm_area_prev->vm_end = vm_area->vm_end;
                        vm_area_prev->vm_next = vm_area->vm_next;
                        dealloc_vm_area(vm_area);
                        vm_area = vm_area_prev->vm_next;
                    }
                    else{
                        vm_area_prev = vm_area;
                        vm_area = vm_area->vm_next;
                        
                    }
                }
            }
        }
    }
    if((found == 0) && (flags == MAP_FIXED )){
        return -EINVAL;
    }
    if((found == 0) ){    
        struct vm_area* vm_area = current->vm_area;
        
        if(vm_area == NULL){
            if(length >= (MMAP_AREA_END - MMAP_AREA_START)){
                return -EINVAL;
            }
            vm_area = alloc_vm_area();
            vm_area->vm_start = MMAP_AREA_START;
            vm_area->vm_end = MMAP_AREA_START + length;
            vm_area->access_flags = prot;
            vm_area->vm_next = NULL;
            ret_addr = vm_area->vm_start;
            current->vm_area = vm_area;
        }
        else{

            if(length <= (vm_area->vm_start - MMAP_AREA_START)){
                struct vm_area* vm_area_new = alloc_vm_area();
                vm_area_new->vm_start = MMAP_AREA_START;
                vm_area_new->vm_end = MMAP_AREA_START + length;
                vm_area_new->access_flags = prot;
                vm_area_new->vm_next = vm_area;
                ret_addr = vm_area_new->vm_start;
                current->vm_area = vm_area_new;
                found = 1;
            }
            else{
                struct vm_area* vm_area_prev = current->vm_area;
                unsigned long current_aligned_addr = vm_area_prev->vm_end;
                vm_area = vm_area->vm_next;

                while(1){
                    if((vm_area == NULL) && (current_aligned_addr + length <= MMAP_AREA_END)){
                        struct vm_area* vm_area_new = alloc_vm_area();
                        vm_area_new->vm_start = current_aligned_addr;
                        vm_area_new->vm_end = current_aligned_addr + length;
                        vm_area_new->access_flags = prot;
                        vm_area_new->vm_next = vm_area;
                        vm_area_prev->vm_next = vm_area_new;
                        ret_addr = vm_area_new->vm_start;
                        found =1;
                        break;
                    }
                    if(current_aligned_addr + length <= vm_area->vm_start){
                        struct vm_area* vm_area_new = alloc_vm_area();
                        vm_area_new->vm_start = current_aligned_addr;
                        vm_area_new->vm_end = current_aligned_addr + length;
                        vm_area_new->access_flags = prot;
                        vm_area_new->vm_next = vm_area;
                        vm_area_prev->vm_next = vm_area_new;
                        ret_addr = vm_area_new->vm_start;
                        found = 1;
                        break;
                    }
                    
                    
                    vm_area_prev = vm_area;
                    vm_area = vm_area->vm_next;
                    current_aligned_addr = vm_area_prev->vm_end;
                }
                if(found == 1){
                    vm_area_prev = current->vm_area;
                    vm_area = vm_area_prev->vm_next;
                    while(vm_area != NULL){
                        if((vm_area_prev->vm_end == vm_area->vm_start) && (vm_area->access_flags == vm_area_prev->access_flags)){
                            vm_area_prev->vm_end = vm_area->vm_end;
                            vm_area_prev->vm_next = vm_area->vm_next;
                            dealloc_vm_area(vm_area);
                            vm_area = vm_area_prev->vm_next;
                        }
                        else{
                            
                            vm_area_prev = vm_area;
                            vm_area = vm_area->vm_next;
                        }
                    }
                    int res = 1;

                    num = 0;
                    vm_area1 = current->vm_area;
                    while (vm_area1)
                    {
                        num ++;
                        vm_area1 = vm_area1->vm_next;
                    }
                    if(num > 128){
                        vm_area_unmap(current,ret_addr,length);
                        return -EINVAL;
                    }

                    if(flags&MAP_POPULATE){
                        map(current->pgd, ret_addr ,prot,length);  
                    } 
                    return ret_addr;
                }
                else{
                    return -EINVAL;
                }
            }
        }
    }  

    num = 0;
    vm_area1 = current->vm_area;
    while (vm_area1)
    {
        num ++;
        vm_area1 = vm_area1->vm_next;
    }

    if(num > 128){
        return -EINVAL;
    }


    int res = 1;
    if(flags&MAP_POPULATE){
        map(current->pgd, ret_addr ,prot,length );  
    } 
    return ret_addr;
}
/**
 * munmap system call implemenations
 */

int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    if(!((addr >= MMAP_AREA_START)&&(addr + length <= MMAP_AREA_END)) ){
        return -EINVAL;
    }

    if(length % PAGE_SIZE != 0){
        length += (PAGE_SIZE-(length%PAGE_SIZE));
    }
    struct vm_area* vm_area = current->vm_area;
    struct vm_area* vm_area_prev = current->vm_area;

    int num = 0;
    vm_area = current->vm_area;
    while (vm_area)
    {
        num ++;
        vm_area = vm_area->vm_next;
    }

    vm_area = current->vm_area;


    if(current->vm_area == NULL){
        return 0;
    }
     while((current->vm_area != NULL) && (!((addr + length <= current->vm_area->vm_start ) || (addr >= current->vm_area->vm_end)))){
        if((current->vm_area->vm_start < addr) && (current->vm_area->vm_end > addr + length)){
                if(num >= 128){
                    return -EINVAL;
                }
                struct vm_area* vm_area_new = alloc_vm_area();
                vm_area_new->vm_start = addr + length;
                vm_area_new->vm_end = current->vm_area->vm_end;
                vm_area_new->access_flags = current->vm_area->access_flags;

                current->vm_area->vm_end = addr;

                vm_area_new->vm_next  = current->vm_area->vm_next;
                current->vm_area->vm_next = vm_area_new;
                break;
            }
            if(vm_area->vm_start < addr){
                current->vm_area->vm_end = addr;
                break;
            }
            else if((vm_area->vm_end <=  addr + length)){
                vm_area = current->vm_area;
                struct vm_area* temp = vm_area->vm_next;
                dealloc_vm_area(vm_area);
                current->vm_area = temp;
                continue;
            }
            else if(vm_area->vm_start <=  addr + length){
                current->vm_area->vm_start = addr + length;
                break;
            }
            break;
     }
    vm_area_prev = current->vm_area;
    if(NULL != vm_area_prev ){
        vm_area = vm_area_prev->vm_next;
        while((vm_area != NULL) && (((addr + length <= vm_area->vm_start ) || (addr >= vm_area->vm_end)))){
            vm_area = vm_area->vm_next;
            vm_area_prev = vm_area_prev->vm_next;
        }
        while(vm_area != NULL){
            if((vm_area->vm_start < addr) && (vm_area->vm_end > addr + length)){
                if(num >= 128){
                    return -EINVAL;
                }
                struct vm_area* vm_area_new = alloc_vm_area();
                vm_area_new->vm_start = addr + length;
                vm_area_new->vm_end = vm_area->vm_end;
                vm_area_new->access_flags = vm_area->access_flags;

                vm_area->vm_end = addr;

                vm_area_new->vm_next  = vm_area->vm_next;
                vm_area->vm_next = vm_area_new;
                break;
            }
            if(vm_area->vm_start < addr){
                vm_area->vm_end = addr;
                vm_area_prev = vm_area;
                vm_area = vm_area->vm_next;
            }
            else if((vm_area->vm_end <=  addr + length)){
                struct vm_area* temp = vm_area->vm_next;
                dealloc_vm_area(vm_area);
                vm_area_prev->vm_next = temp;
                vm_area = temp;
            }
            else if(vm_area->vm_start <=  addr + length){
                vm_area->vm_start = addr + length;
                vm_area_prev = vm_area;
                vm_area = vm_area->vm_next;
                break;
            }
            break;
            
        }
    }
    unmap(current,addr,length);
    return 0;

    int isValid = -1;
    return isValid;
}
