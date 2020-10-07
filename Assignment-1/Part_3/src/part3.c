
// Saketh Maddamsetty , 170612

#include <stdio.h>
#include <dirent.h>
#include<string.h>
#include<errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>


/*
    get_size() function takes path of the file/ directory 
    as input and outputs the sizes of the directory as well 
    as its immediate childs.
*/
int get_size(char *filename){
    int res_size = 0;
    DIR *dir;
    struct stat file_stat;
    struct dirent * entry;

    if ( (dir = opendir(filename)) == NULL){
        perror("opendir() error");
    }

    else{
        while((entry = readdir(dir)) != NULL){
            if(entry->d_name[0] != '.'){ // checks if d_name is "." or ".." .
                char buf_filename[512];
                strcpy(buf_filename,filename);
                strcat(buf_filename,"/");
                strcat(buf_filename,entry->d_name); // path of all the childs withing directory is created.
                stat(buf_filename,&file_stat);
                if(S_ISDIR(file_stat.st_mode)){  // if directory then recursively calls the function.
                    res_size += get_size(buf_filename);
                }
                else{
                    res_size += file_stat.st_size;
                }
            }
        }
    }
    return res_size;
}
/*
    converts an integer number to its character form.
*/
char* int_to_char(int num){
    char*  num1 = (char *) malloc (100 * sizeof(char));
    int c = 0;
    while(num != 0){
        num1[c] = (num % 10) + '0';
        num /= 10;
        c += 1;
    }
    num1[c] = '\0';
    return num1;
}
/*
    converts a number's character form back to integer.
*/
int char_to_int ( char* num){
    int ans = 0;
    int c = 0;
    int ten = 1;
    while(num[c] != '\0'){
        ans += (num[c]-'0')*ten;
        ten *= 10;
        c += 1;
    }
    return ans;
}

/*
    file() function given a path to dircetory return the name of current dircetory.
*/

char *  file(char * filename){
    DIR *dir;
    struct stat file_stat;
    struct dirent * entry;
    struct stat file_stat2;
    struct dirent * entry2;
    char f[250];
    strcpy(f,filename);
    stat(filename,&file_stat);
    strcat(f,"/../");
    if ( (dir = opendir(f)) == NULL){
        perror("opendir() error");
    }
    while((entry = readdir(dir)) != NULL){
        if(entry->d_ino == file_stat.st_ino){ // two directories are same if thier inodes are same
            char * file = (char *) malloc (250 * sizeof(char));
            strcpy(file,entry->d_name);
            return file;
        }
    }
    return NULL;
}

/*
    size_childs splits into n+1 processes for n childs and excecutes the function get_size()
*/
int size_childs(char *filename){
    int res_size = 0;
    int flag = 0;
    DIR *dir;
    struct stat file_stat;
    struct dirent * entry;
    if ( (dir = opendir(filename)) == NULL){
        perror("opendir() error");
    }
    else{
        while((entry = readdir(dir)) != NULL){
            if(entry->d_name[0] != '.'){
                char buf_filename[512];
                strcpy(buf_filename,filename);
                strcat(buf_filename,"/");
                strcat(buf_filename,entry->d_name);
                stat(buf_filename,&file_stat);
                if(S_ISDIR(file_stat.st_mode)){ // if the file is a directory then fork to create a child process.
                    int fd[2];
                    if(pipe(fd) < 0 ){
                        perror("pipe");
                        exit(-1);
                    }
                    int pid = fork();
                    if(pid < 0){
                        perror("fork() error");
                    }
                    // child process
                    if(pid == 0){
                        close(fd[0]);
                        res_size = get_size(buf_filename); // get the size of child dircetory using get_size() defined above.
                        char * num = int_to_char(res_size); // number is converted to char representation for sending it over queue.
                        if(write(fd[1],num,90) < 0){
                            perror("error in write()");
                        }
                        printf("%s %d\n",entry->d_name,res_size);
                        return 0;
                    }
                    // parent process
                    else{
                        close(fd[1]);
                        wait(NULL);
                        char num[100];
                        if(read(fd[0],num,90) < 0){ // reading characters from queue.
                            perror("read() error");
                        }
                        int i_num = char_to_int(num); // interpreting integer using mention function.
                        res_size += i_num; // the size of root node
                    }
                    
                }
                else{
                    res_size += file_stat.st_size;
                }
            }
        }
    }
    printf("%s %d\n",file(filename),res_size);
    return res_size;
}

int main( int argc, char *argv[] ){

    if(argc != 2){
        printf("number of inputs doesn't match\n");
        return 0;
    }
    size_childs(argv[1]);
    
    return 0;
}