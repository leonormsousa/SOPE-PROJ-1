#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "flags.h"
#include "utils.h"

// TO-DO: mais tarde mudar esta função (e outras) para outro ficheiro com respetivo header
//adicionar também verificação de argumentos válidos
struct argFlags arg_parser(int argc, char** argv) {
    struct argFlags arg_flags = {0};
    char *token;
    int i;
    int r_can=1, h_can=1, o_can=1;
    for(i=1; i<argc-1; i++) {
        if(r_can && (strcmp(argv[i],"-r")==0)) {
            arg_flags.dir_full_search=1;
        }

        else if(h_can && (strcmp(argv[i],"-h")==0)) {
            r_can=0;
            arg_flags.hash_calc=1;
            i++;
            token = strtok(argv[i],",");
            while(token!=NULL) {
                if(strcmp(token,"md5")==0)
                    arg_flags.hash_flags.md5=1;
                else if(strcmp(token,"sha1")==0)
                    arg_flags.hash_flags.sha1=1;
                else if(strcmp(token,"sha256")==0)
                    arg_flags.hash_flags.sha256=1;
                token = strtok(NULL,",");
            }
        }

        else if(o_can && (strcmp(argv[i],"-o")==0)) {
            r_can=0;
            h_can=0;
            arg_flags.outfile=1;
            i++;
            strcpy(arg_flags.outfile_name,argv[i]);
        }

        else if(strcmp(argv[i],"-v")==0) {
            r_can=0;
            h_can=0;
            o_can=0;
            if(getenv("LOGFILENAME")!=NULL ) {
                strcpy(arg_flags.logfile_name,getenv("LOGFILENAME")); 
                arg_flags.logfile=1;
            }
            else perror("Environment variable not set. -v argument will be ignored\n\n\n");
        }

        else
        {
            perror("To use the forensic tool, use the following syntax:\nforensic [-r] [-h [md5[,sha1[,sha256]]] [-o <outfile>] [-v] <file|dir>\n");      
            exit(2);
        }
    }
    if (i>=argc)
    {
        perror("Missing file's name.\n");
        exit(3);
    }
    strcpy(arg_flags.path,argv[argc-1]);
    return arg_flags;
}

//falta adicionar verificação de file_type mas como será usando "file",
//invés da "stat", fica para depois
void print_file_data(const char* path, struct argFlags arg_flags) {
    struct stat fs; //fs: file_stat
    stat(path,&fs);
    int fd_outfile;
    //checking for output file
    if (arg_flags.outfile)
    {
        printf("Data saved on file %s\n", arg_flags.outfile_name);
        printf("Execution records saved on file ...\n");
        fd_outfile=open(arg_flags.outfile_name, O_WRONLY | O_APPEND | O_CREAT);
        if (fd_outfile<0)
        {
            printf("Error on opening output file.\n");
            exit(10);
        }
        dup2(fd_outfile, STDOUT_FILENO);
    }

    //prints file name
    printf("%s,",path);

    //prints file type
    int fd[2];
    pipe(fd);
    int pid=fork();
    if (pid<0)
    {
        perror("Error ocured in fork.\n");
        exit(4);
    }
    if (pid==0)
    {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        execlp("file", "file", path, NULL);
        perror("Error on call to file.\n");
        exit(5);
    }
    close(fd[1]);
    char buf[100];
    read(fd[0], &buf, 100);
    int l=strlen(buf);
    memmove(buf, buf + strlen(path)+2, l-strlen(path)-2+1);
    buf[strlen(buf)-1]='\0';
    printf("%s,",buf);
    wait(NULL);

    //prints file size in bytes
    printf("%ld,",fs.st_size);

    //prints file permissions/access
    printf( (fs.st_mode & S_IRUSR) ? "r" : "");
    printf( (fs.st_mode & S_IWUSR) ? "w" : "");
    printf( (fs.st_mode & S_IXUSR) ? "x" : "");
    printf(",");

    //prints dates
    struct tm *at;
    at=gmtime(&fs.st_atime);
    printf("%d-%d-%dT%d:%d:%d,", at->tm_year, at->tm_mon, at->tm_mday, at->tm_hour, at->tm_min, at->tm_sec);
    struct tm *mt;
    mt=gmtime(&fs.st_mtime);
    printf("%d-%d-%dT%d:%d:%d", mt->tm_year, mt->tm_mon, mt->tm_mday, mt->tm_hour, mt->tm_min, mt->tm_sec);
    
    //running hash calculations
    if(arg_flags.hash_calc) {
        char hash[300];
        if(arg_flags.hash_flags.md5) {
            printf(",");
            char *md5cmd = malloc(strlen("md5sum ")+strlen(path)+1);
            strcpy(md5cmd,"md5sum ");
            strcat(md5cmd,path);
            char hash[300];
            FILE *cmd_out = popen(md5cmd,"r");
            fscanf(cmd_out,"%s",hash);
            printf("%s",hash);
        }
        if(arg_flags.hash_flags.sha1) {
            printf(",");
            char *sha1cmd = malloc(strlen("sha1sum ")+strlen(path)+1);
            strcpy(sha1cmd,"sha1sum ");
            strcat(sha1cmd,path);
            FILE *cmd_out = popen(sha1cmd,"r");
            fscanf(cmd_out,"%s",hash);
            printf("%s",hash);  
        }
        if(arg_flags.hash_flags.sha256) {
            printf(",");
            char *sha256cmd = malloc(strlen("sha256sum ")+strlen(path)+1);
            strcpy(sha256cmd,"sha256sum ");
            strcat(sha256cmd,path);
            FILE *cmd_out = popen(sha256cmd,"r");
            fscanf(cmd_out,"%s",hash);
            printf("%s",hash);  
        }
    }

    printf("\n");
    if (arg_flags.outfile)
        close(fd_outfile);
    return;
}