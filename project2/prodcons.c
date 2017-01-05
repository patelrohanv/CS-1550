/* 
 */
#include <linux/prodcons.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

//#include <linux/prodcons.h>

#define MAP_SIZE 0x00000FFF

void *BASE_PTR;
void *TEMP_PTR;


int main(int aegc, char *argv[])
{
     //take in values from command line
     int producer = atoi(argv[1]);
     int consumer = atoi(argv[2]);
     int buffer = atoi(argv[3]);
     int wait_var;
     BASE_PTR = (void *) mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
     if(BASE_PTR == (void *) -1) 
     {
          fprintf(stderr, "Error mapping memory\n");
          exit(1);
     }
     
     int *base_ptr = BASE_PTR;
     int *new_ptr;
     int *curr_ptr = BASE_PTR;
     int size = sizeof(struct cs1550_sem);
     curr_ptr = curr_ptr + size;
     if(curr_ptr > base_ptr + MAP_SIZE) 
     {
          fprintf(stderr, "Address out of range\n");
          exit(1);
     }
     else
     {
          new_ptr = curr_ptr - size;
     }

     //semaphore for empty
     //empty(n)
     struct cs1550_sem *empty = (struct cs1550_sem *) new_ptr;
     empty ->value = buffer;
     empty -> head = NULL;
     empty -> tail = NULL;
     //semaphore for full
     //full(0)
     struct cs1550_sem *full = (struct cs1550_sem *) new_ptr + 1;
     full -> value = 0;
     full -> head = NULL;
     full -> tail = NULL;
     //semaphore for mutex
     //mutex(1)
     struct cs1550_sem *mutex = (struct cs1550_sem *) new_ptr + 2;
     mutex -> value = 1;
     mutex -> head = NULL;
     mutex -> tail = NULL;

     TEMP_PTR = (void *) mmap(NULL, sizeof(int)*(buffer + 1), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

     int *counter = (int*)TEMP_PTR;
     counter = buffer;
     int *producer_num = (int*)TEMP_PTR + 1;
     producer_num = 0;
     int *consumer_num = (int*)TEMP_PTR + 2;
     consumer_num = 0;
     int *buffer_pos = (int*)TEMP_PTR + 3;

     //producer
     int i = 0;
     while (i < producer){
          if(fork() == 0){
               int in;
               while(1){
                    //producing pancakes
                    cs1550_down(empty);
                    cs1550_down(mutex);
                    in = producer_num;
                    buffer_pos[*producer_num] = in;
                    printf("Chef %c Produced Pancake: %d\n", i+65, in);
                    *producer_num = (*producer_num+1) % *counter;
                    cs1550_up(mutex);
                    cs1550_up(full);
               }
          }
          i++;
     }

     //consumer
     j = 0;
     while (i < consumer && j <  i){
          if (fork() == 0){
               int out;
               while(1){
                    //consuming pancakes
                    cs1550_up(full);
                    cs1550_down(mutex);
                    out = consumer_num;
                    buffer_pos[*consumer_num] = out;
                    printf("Customer %c Ate Pancake: %d\n", i+65,out);
                    i--;
                    *consumer_num = (*consumer_num+1) % *counter;
                    cs1550_up(mutex);
                    cs1550_up(empty);
               }
          }
          j++;
     }

     //consumer
     /*
     sem->value = 0;
     printf("Base pointer (0x%08x), Current pointer (0x%08x), New pointer (0x%08x)\n", base_ptr, curr_ptr, new_ptr);
     printf("Base pointer (%d), Current pointer (%d), New pointer (%d)\n", *base_ptr, *curr_ptr, *new_ptr);
     cs1550_down(sem);
     sleep(5);
     printf("Semaphore value (%d)\n", sem->value);
     printf("Base pointer (%d), Current pointer (%d), New pointer (%d)\n", *base_ptr, *curr_ptr, *new_ptr);
     cs1550_up(sem);
     printf("Semaphore value (%d)\n", sem->value);
     printf("Base pointer (%d), Current pointer (%d), New pointer (%d)\n", *base_ptr, *curr_ptr, *new_ptr);
     */
     wait(&wait_var);
     return 0;
}

void cs1550_down(struct cs1550_sem *sem) 
{
     syscall(__NR_sys_cs1550_down, sem);
}

void cs1550_up(struct cs1550_sem *sem) 
{
     syscall(__NR_sys_cs1550_up, sem);
}
