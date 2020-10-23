/*
Ayush Tharwani
170201
*/
#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
/***********************************************************************
 * Use this function to allocate pipe info && Don't Modify below function
 ***********************************************************************/
struct pipe_info* alloc_pipe_info()
{
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);
    pipe ->pipe_buff = buffer;
    return pipe;
}


void free_pipe_info(struct pipe_info *p_info)
{
    if(p_info)
    {
        os_page_free(OS_DS_REG ,p_info->pipe_buff);
        os_page_free(OS_DS_REG ,p_info);
    }
}
/*************************************************************************/
/*************************************************************************/


int pipe_read(struct file *filep, char *buff, u32 count)
{   
    if(buff == NULL){
        return -EINVAL;
    }
    if(!filep){
        return -EINVAL;
    }
    if((filep->pipe->is_ropen == 0) || (filep->mode != O_READ)){
        return -EACCES;
    }
    if(count > filep->pipe->buffer_offset){
        count = filep->pipe->buffer_offset;
    }
    int num = filep->pipe->read_pos;
    int num2 = 0;
    while(num2 < count){
        buff[num2] = filep->pipe->pipe_buff[num % 4096];
        num2 ++;
        num ++;
    }
    filep->pipe->read_pos = num % 4096;
    filep->pipe->buffer_offset = filep->pipe->buffer_offset - count;
    return count;
    /**
    *  TODO:: Implementation of Pipe Read
    *  Read the contect from buff (pipe_info -> pipe_buff) and write to the buff(argument 2);
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
}


int pipe_write(struct file *filep, char *buff, u32 count)
{   
    if(buff == NULL){
        return -EINVAL;
    }
    if(!filep){
        return -EINVAL;
    }
    if((filep->pipe->is_wopen == 0 ) || (filep->mode != O_WRITE)){
        return -EACCES;
    }
    if((count + filep->pipe->buffer_offset) > 4096 ){
        return -EOTHERS;
    }
    int num = filep->pipe->write_pos;
    int num2 = 0;
    while(num2 < count){
        filep->pipe->pipe_buff[num % 4096] = buff[num2];
        num2 ++;
        num ++;
    }
    filep->pipe->write_pos = num%4096;
    filep->pipe->buffer_offset = count + filep->pipe->buffer_offset;
    return count;

    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the contect from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
}

int create_pipe(struct exec_context *current, int *fd)
{
    if(!current){
        return -EINVAL;
    }
    if(fd == NULL){
        return -EINVAL;
    }
    struct file *file_object = alloc_file();
    if(!file_object){
        return -ENOMEM;
    }
    file_object->pipe = alloc_pipe_info();
    if(!file_object->pipe){
        return -ENOMEM;
    }
    struct file *file_object2 = alloc_file();
    if(!file_object2){
        return -ENOMEM;
    }
    int fd1 = 3;
    while(current->files[fd1]){
        fd1 ++;
        if(fd1 >= 32){
            return -EOTHERS;
        }
    }
    fd[0] = fd1;
    current->files[fd1] = file_object;
    fd1 ++;
    while(current->files[fd1]){
        if(fd1 >= 32){
            return -EOTHERS;
        }
        fd1 ++;
    }
    fd[1] = fd1;
    current->files[fd1] = file_object2;


    file_object->type = 4;
    file_object->ref_count = 1;
    file_object->pipe->is_ropen = 1;
    file_object->pipe->is_wopen = 1;
    file_object->pipe->buffer_offset = 0;
    file_object->fops->write = pipe_write;
    file_object->fops->read = pipe_read;
    file_object->fops->close = generic_close;
    file_object->pipe->read_pos = 0;
    file_object->pipe->write_pos = 0;
    file_object->mode = O_READ;
    file_object->offp = 0;

    file_object2->offp = 0;
    file_object2->pipe = file_object->pipe;
    file_object2->type = 4;
    file_object2->ref_count = 1;
    file_object2->mode = O_WRITE;
    file_object2->fops->write = pipe_write;
    file_object2->fops->read = pipe_read;
    file_object2->fops->close = generic_close;
    return 0;

    
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */
}

