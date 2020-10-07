#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<common.h>

/* XXX NOTE XXX  
       Do not declare any static/global variables. Answers deviating from this 
       requirement will not be graded.
*/
void init_rwlock(rwlock_t *lock)
{
   lock->value = 0x1000000000000;
   /*Your code for lock initialization*/
}

void write_lock(rwlock_t *lock)
{
   
      // int val = 1; 
      // atomic_add(&(lock->value),6);
      // printf("val = %ld \n", lock->value);
   // while(lock->value);
   // atomic_add(&(lock->value),0x1000000000000);
   while(1){
      if(atomic_add(&(lock->value),-0x1000000000000) == 0 ){
         // atomic_add(&(lock->value),0x1000000000000);
         break;
      }
      else{
         atomic_add(&(lock->value),0x1000000000000);
      }
   }
   // printf("val = %ld \n", lock->value);

}

void write_unlock(rwlock_t *lock)
{
   while(1){
      if(atomic_add(&(lock->value),-1) < 0){
         atomic_add(&(lock->value),0x1000000000001);
         break;
      }
      else{
         atomic_add(&(lock->value),1);
      }
   }
   /*Your code to release the write lock*/
}


void read_lock(rwlock_t *lock)
{
   while(1){
      if(atomic_add(&(lock->value),-1) > 0){
         break;
      }
      else{
         atomic_add(&(lock->value),1);
      }
   }
   /*Your code to acquire read lock*/
}

void read_unlock(rwlock_t *lock)
{  
   atomic_add(&(lock->value),1);
   if(lock->value > 0x1000000000000){
      lock->value =  0x1000000000000;
   }
}
