#include <stdio.h>
#include <dirent.h>
#include<string.h>
#include<errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#define UP_LIMIT 510


void grep(char * filepath ,char * regex);

// funtionality for recursively traversing through all sub-directories/ files.
int transverse(char *filename , char* regex){
    DIR *dir;
    struct stat file_stat;
    struct dirent * entry;
    stat(filename,&file_stat);
    if(S_ISREG(file_stat.st_mode)){ // if entered path corresponds to file itself.
        grep(filename,regex);
        return 0;
    }
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
                if(S_ISDIR(file_stat.st_mode)){ // checks if the file is dircetory
                    transverse(buf_filename,regex);
                }
                else{
                    grep(buf_filename,regex); //  if file is reg file.
                }
            }
        }
    }
    return 0;
}


// grep funtionality implementation.
void grep(char * filepath ,char * regex){
    int fd = open(filepath,O_RDONLY);
    if(fd < 0){
        perror("error in opening file.");
    }
    int outsize;
    int bufsize = 32;
    char * buf = (char *) malloc(bufsize * sizeof(char));
    int linesize = 64;
    char * line =  (char *) malloc(linesize * sizeof(char));
    int flag = 1;
    int anscount = 0;
    int flag2 = 0;
    int last = 0;
    int count = 0;
    int prev = 0;
    while(flag){
        if((outsize = (read(fd,buf,bufsize-1))) > 0){
            buf[outsize] = '\0';
            count = 0;
            last = 0;
            if(flag2 == 2){ // flag2 implies that there is some characters in variable line from preveious iteration of while loop.
                while(count < outsize & buf[count] != '\n' & buf[count] != '\0'){
                    count += 1;
                }
                if(count == outsize){ // implies there was no newline character in this read call, so we have to double the size of buf and line variables
                    bufsize *= 2;
                    linesize *= 2;
                    char * bufnew = (char *) malloc(bufsize*sizeof(char));
                    char * linenew = (char *) malloc(linesize*sizeof(char));
                    strncpy(linenew,line,prev);
                    free(line);
                    line = linenew;
                    strncat(line,buf,count);
                    prev += count;
                    free (buf);
                    buf = bufnew;
                    continue;
                }
                strncat(line,buf,count);
                line[count+prev] = '\0';
                if(strstr(line,regex) != NULL){ // if the line contain a word regex, then strstr return a pointer to first occurence if pattern (word).
                    printf("\033[1;31m"); // coloring output in bash.
                    printf("%s:",filepath);
                    printf("\033[0m");
                    printf("%s\n", line);
                }
                last = count + 1;
                count += 1;
                flag2 = 0;
            }
            // now there is no pending characters from previous iteration.
            while(count < outsize){
                if(buf[count] == '\n'){ // as soon as a newline character appears gather this line in 'line' variable.
                    if(last == count){
                        count += 1;
                        last += 1;
                        continue;
                    }
                    strncpy(line,buf+last,count-last);
                    line[count-last] = '\0';
                    last = count + 1;
                    if(strstr(line,regex) != NULL){// string search for pattern using strstr
                        printf("\033[1;31m");
                        printf("%s:",filepath);
                        printf("\033[0m");
                        printf("%s\n", line);
                    }
                }
                count ++;
            }
            if(count > last){ // if the while loop is terminated but some characters are left after the last newline, these are evaluated in the next ietration with the help of flag2.
                strncpy(line,buf+last,count-last+1);
                prev = count-last+1;
                flag2 = 2; 
            }
        }   
        else {
            flag = 0;
        } 
    }
    return;
}

int main( int argc, char *argv[] ){
    if(argc != 3){
        printf("number of inputs doesn't match\n");
        return 0;
    }
    transverse(argv[2],argv[1]);
    return 0;
}

