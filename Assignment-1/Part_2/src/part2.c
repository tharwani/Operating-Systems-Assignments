
// Ayush Tharwani , 170201


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<fcntl.h> 
#include<errno.h>  
#include<string.h>

// function corresponding to symbol @. ie, grep string path | wc -l
int word_count(char * string , char * path){
    int fd[2];
    if(pipe(fd) < 0){
        perror("pipe");
        exit(-1);
    }
    int pid = fork();
    if(pid < 0 ){
        perror("fork()  error");
        return -1;
    }
    if(pid == 0){
        // child process
        int fd_new = dup2(fd[1],1); // connecting fd 1 to the file object pointed by fd[1] which is write end of queue.
        close(fd[0]);
        execlp("grep", "grep", "-rF", string , path , NULL);
    }
        // parent process
    else{
        int fd_new = dup2(fd[0],0);
        close(fd[1]);
        execlp("wc", "wc", "-l", NULL);
    }
    return 0;
}

// function corresponding to $.
int tee(char * string , char * path , char * output_file , char ** command){
    int fd[2]; // for first pipe
    if(pipe(fd) < 0){
        perror("pipe");
        exit(-1);
    }
    int pid = fork();
    if(pid < 0 ){
        perror("fork()  error");
        return -1;
    }
    if(pid == 0){
        // child process with functionality of grep.
        int fd_new = dup2(fd[1],1);
        close(fd[0]);
        execlp("grep", "grep", "-rF", string , path , NULL);
    }
        // parent process
    else{
        int fd_new = dup2(fd[0],0);
        close(fd[1]);
        int fd1[2]; // for second pipe
        if(pipe(fd1) < 0){
            perror("pipe");
            exit(-1);
        }
        int pid1 = fork();
        if(pid1 < 0 ){
            perror("fork()  error");
            return -1;
        }
        // child process with functionality of tee.
        if(pid1 == 0){
            dup2(2,1);
            dup2(fd1[1],1);
            close(fd1[0]);
            int file_des = open(output_file, O_WRONLY | O_CREAT,0666);
            char buf[1024];
            int size_read;
            while((size_read = read(0,buf,1023)) > 0){ 
                if(size_read == 1023){
                    write(file_des,buf,1023);
                    write(1,buf,1023);
                }
                else{
                    write(file_des,buf,size_read);
                    write(1,buf,size_read);
                    break;
                }
            }
        }
        else{
            dup2(fd1[0],0);
            close(fd1[1]);
            char* fields[10];
            int fcount = 0;
            int count = 0;
            int last  = 0;
            execvp(command[0] , command);
        }
        
    }
    return 0;
}


int main( int argc, char *argv[]){
    if(argc < 4){
        printf("number of inputs doesn't match\n");
        return 0;
    }
    if(argv[1][0] == '$'){
        if(argc < 6){
            printf("number of inputs doesn't match for this operation\n");
            return 0;
        }
        tee(argv[2],argv[3],argv[4],&argv[5]);
    }
    else if (argv[1][0] == '@')
    {
        word_count(argv[2],argv[3]);
    }
    else{
        printf("enter appropriate symbol\n");
    }
    return 0;
}
