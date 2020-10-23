/*
Ayush Tharwani
170201

*/

#include <types.h>
#include <context.h>
#include <file.h>
#include <lib.h>
#include <serial.h>
#include <entry.h>
#include <memory.h>
#include <fs.h>
#include <kbd.h>
#include <pipe.h>

/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/
void free_file_object(struct file *filep)
{
  if (filep)
  {
    os_page_free(OS_DS_REG, filep);
    stats->file_objects--;
  }
}

struct file *alloc_file()
{

  struct file *file = (struct file *)os_page_alloc(OS_DS_REG);
  file->fops = (struct fileops *)(file + sizeof(struct file));
  bzero((char *)file->fops, sizeof(struct fileops));
  stats->file_objects++;
  return file;
}

static int do_read_kbd(struct file *filep, char *buff, u32 count)
{
  kbd_read(buff);
  return 1;
}

static int do_write_console(struct file *filep, char *buff, u32 count)
{
  struct exec_context *current = get_current_ctx();
  return do_write(current, (u64)buff, (u64)count);
}

struct file *create_standard_IO(int type)
{
  struct file *filep = alloc_file();
  filep->type = type;
  if (type == STDIN)
    filep->mode = O_READ;
  else
    filep->mode = O_WRITE;
  if (type == STDIN)
  {
    filep->fops->read = do_read_kbd;
  }
  else
  {
    filep->fops->write = do_write_console;
  }
  filep->fops->close = generic_close;
  filep->ref_count=1;
  return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
  int fd = type;
  struct file *filep = ctx->files[type];
  if (!filep)
  {
    filep = create_standard_IO(type);
  }
  else
  {
    filep->ref_count++;
    fd = 3;
    while (ctx->files[fd])
      fd++;
  }
  ctx->files[fd] = filep;
  return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/

void do_file_fork(struct exec_context *child)
{
  if (!child)
  {
    return;
  }
  int i = 0;
  for (; i < 32; i++)
  {
    if (child->files[i])
    {
      child->files[i]->ref_count++;
    }
  }

  /*TODO the child fds are a copy of the parent. Adjust the refcount*/
}

void do_file_exit(struct exec_context *ctx)
{
  if (!ctx)
  {
    return;
  }
  int i = 0;
  for (; i < 32; i++)
  {
    if (ctx->files[i])
    {
      generic_close(ctx->files[i]);
    }
  }
  /*TODO the process is exiting. Adjust the ref_count
     of files*/
}

long generic_close(struct file *filep)
{
  /** TODO Implementation of close (pipe, file) based on the type 
   * Adjust the ref_count, free file object
   * Incase of Error return valid Error code 
   */
  if (!filep)
  {
    return -EINVAL;
  }
  if (filep->type == 4)
  {
    if (filep->ref_count > 1)
    { 
      filep->ref_count--;
      return 0;
    }
    if (filep->ref_count == 1)
    { 
      if((filep->mode == O_READ) && (filep->pipe->is_ropen) ){
        filep->pipe->is_ropen = 0;
      }
      if((filep->mode == O_WRITE) && (filep->pipe->is_wopen)){
        filep->pipe->is_wopen = 0;
      }
      
      if((filep->pipe->is_ropen == 0)&&(filep->pipe->is_wopen==0)){
        free_pipe_info(filep->pipe);
      }
      free_file_object(filep);
      return 0;
    }
  }
  if((filep->type <= 3)&&(filep->type >= 0)){
    if (filep->ref_count > 1)
    {
      filep->ref_count--;
      return 0;
    }
    if (filep->ref_count == 1)
    {
      free_file_object(filep);
      return 0;
    }
  }
  int ret_fd = -EINVAL;
  return ret_fd;
}

static int do_read_regular(struct file *filep, char *buff, u32 count)
{
  if ((filep == NULL)||(buff == NULL))
  {
    return -EINVAL;
  }
  if ((filep->mode & O_READ) != O_READ)
  {
    return -EACCES;
  }

  if (filep->inode->file_size -  filep->offp < count){
      count = filep->inode->file_size - filep->offp;
      // return -EOTHERS;
  }

  int bytes_read = flat_read(filep->inode, buff, count, &(filep->offp));

  if (bytes_read < 0)
  {
    return -EOTHERS;
  }
  filep->offp += bytes_read;
  return bytes_read;
  /** TODO Implementation of File Read, 
    *  You should be reading the content from File using file system read function call and fill the buf
    *  Validate the permission, file existence, Max length etc
    *  Incase of Error return valid Error code 
    * */
}

static int do_write_regular(struct file *filep, char *buff, u32 count)
{
  if ((filep == NULL)||(buff == NULL)||(count == 0))
  {
    return -EACCES;
  }
  if(4096- filep->offp < count){
      return -EOTHERS;
  }
  if ((filep->mode & O_WRITE) != O_WRITE)
  {
    return -EACCES;
  }
  int bytes_wrote = flat_write(filep->inode, buff, count, &(filep->offp));

  if (bytes_wrote < 0)
  {
    return -EOTHERS;
  }
  filep->offp += bytes_wrote;
  return bytes_wrote;

  /** TODO Implementation of File write, 
    *   You should be writing the content from buff to File by using File system write function
    *   Validate the permission, file existence, Max length etc
    *   Incase of Error return valid Error code 
    * */
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
  /** TODO Implementation of lseek 
    *   Set, Adjust the ofset based on the whence
    *   Incase of Error return valid Error code 
    * */
  if (!filep)
  {
    return -EINVAL;
  }
  
  if (whence == SEEK_SET)
  {
    if (offset > filep->inode->e_pos-filep->inode->s_pos)
    {
      return -EINVAL;
    }
    if (offset < 0)
    {
      return -EINVAL;
    }
    filep->offp = offset;
  }

  else if (whence == SEEK_CUR)
  {
    if (offset + filep->offp > filep->inode->e_pos-filep->inode->s_pos)
    {
      return -EINVAL;
    }
    if (offset + filep->offp < 0)
    {
      return -EINVAL;
    }
    filep->offp = offset + filep->offp;
  }

  else if (whence == SEEK_END)
  {
    if (offset + filep->inode->file_size > filep->inode->e_pos-filep->inode->s_pos)
    {
      return -EINVAL;
    }
    if (offset + filep->inode->file_size < 0)
    {
      return -EINVAL;
    }
    filep->offp = offset + filep->inode->file_size;
  }
  else{
    return -EINVAL;
  }
  return filep->offp;
}

extern int do_regular_file_open(struct exec_context *ctx, char *filename, u64 flags, u64 mode)
{
  if(!ctx || (filename == NULL) || !flags){
      return -EINVAL;
  }
  struct inode *file_inode = lookup_inode(filename);
  if (file_inode == NULL)
  {
    if ((flags & O_CREAT) == O_CREAT)
    {
      file_inode = create_inode(filename, mode);
      flags = mode;
    }
    else
    {
      return -(EINVAL);
    }
  }
  if(!file_inode){
      return -EINVAL;
  }

  if(file_inode->file_size > 4096){
      return -EOTHERS;
  }

  if ((((file_inode->mode) & O_READ) != O_READ ) && ((flags & O_READ) == O_READ))
  {
    return -EACCES;
  }
  if ((((file_inode->mode) & O_WRITE) != O_WRITE ) && ((flags & O_WRITE) == O_WRITE))
  {
    return -EACCES;
  }
  struct file *file_object = alloc_file();
  if(!file_object){
      return -ENOMEM;
  }
  file_object->type = 3;
  file_object->ref_count = 1;
  file_object->offp = 0;
  file_object->fops->read = do_read_regular;
  file_object->fops->write = do_write_regular;
  file_object->fops->lseek = do_lseek_regular;
  file_object->fops->close = generic_close;
  file_object->inode = file_inode;
  file_object->mode = flags;
  int i = 3;
  while (ctx->files[i])
  {
    i++;
    if(i >= 32){
        return -EOTHERS;
    }
  }
  ctx->files[i] = file_object;
  return i;
  /**  TODO Implementation of file open, 
    *  You should be creating file(use the alloc_file function to creat file), 
    *  To create or Get inode use File system function calls, 
    *  Handle mode and flags 
    *  Validate file existence, Max File count is 32, Max Size is 4KB, etc
    *  Incase of Error return valid Error code 
    * */
  // return i;
}

int fd_dup(struct exec_context *current, int oldfd)
{
  /** TODO Implementation of dup 
      *  Read the man page of dup and implement accordingly 
      *  return the file descriptor,
      *  Incase of Error return valid Error code 
      * */
  if (current == NULL)
  {
    return -EINVAL;
  }
  if ((oldfd < 0) || (oldfd >= 32))
  {
    return -EINVAL;
  }
  if (!(current->files[oldfd]))
  {
    return -EINVAL;
  }
  int fd = 0;
  while (current->files[fd])
  {
    fd++;
    if(fd >= 32){
        return -EOTHERS;
    }
  }
  current->files[fd] = current->files[oldfd];
  current->files[fd]->ref_count++;
  return fd;
}

int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
  /** TODO Implementation of the dup2 
    *  Read the man page of dup2 and implement accordingly 
    *  return the file descriptor,
    *  Incase of Error return valid Error code 
    * */
  if (!(current))
  {
    return -EINVAL;
  }
  if ((oldfd < 0) || (oldfd >= 32))
  {
    return -EINVAL;
  }
  if ((newfd < 0) || (newfd >= 32))
  {
    return -EINVAL;
  }
  if (!(current->files[oldfd]))
  {
    return -EINVAL;
  }
  if (current->files[newfd])
  {
    generic_close(current->files[newfd]);  
    current->files[newfd] = current->files[oldfd];
  }
  else
  {
    current->files[newfd] = current->files[oldfd];
  }
  current->files[oldfd]->ref_count ++;
  return newfd;
  int ret_fd = -EINVAL;
  return ret_fd;
}
