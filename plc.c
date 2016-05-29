
/* I pledge my honor that I have abided by the Stevens Honor System
 * Matthew Weiss
 * */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#define OPTS "a"


static void usage()
{
    printf("Usage: plc <file name> <number of processes>\n"

           );
}

int count(unsigned int start_bytes, unsigned int byte_offset, char *file, FILE *tmp){
    /*VARIABLES
     * line_count_str is used to write our output to a tmp file
     * r is the status of fread
     * j is used for our for loop
     * line_count keeps track of the lines before we write to file
     * buffer is where we read into
     * buffer size is the size of our buffer
     * fp is the file we open
     *
     * */
    char line_count_str[16];
    int r;
    int j;
    FILE *fp;
    unsigned int line_count = 0;
    char buffer[4096];
    int buffer_size = 4096;

    fp = fopen(file , "r");
    /* if file is NULL, we have an error */
    if(fp == NULL) {
        fprintf(stderr,"Error opening file");
        return 1;
    }
    /* go the part of the file we are counting from */
    fseek(fp, start_bytes, SEEK_SET);

    /* while there are still bytes we are assigned to us, loop through the file */
    while(byte_offset > 0) {
        /* if our offest is bigger than our buffer
         * subtract from that and read the buffer
         * else set our buffer size to our offset
         * so we don't read lines that were not intended to be read
         * by this process
         * set the byte_offset to 0 to terminate the loop at the end of this run
         * */

        if (byte_offset > buffer_size){
            byte_offset = byte_offset - buffer_size;
        }else {
            buffer_size = byte_offset;
            byte_offset = 0;
        }
        /* if we reach end of file, stop reading */
        if(r = fread(buffer, buffer_size, 1, fp) == 0){
            break;
        }

        /* search through our buffer for newline characters
         * incrementing when we find one
         * */
        for(j = 0; j < buffer_size; j++){
            if(buffer[j] == '\n'){
                line_count++;
            }
        }

    }

    /*write the line count to a temp file
     * to be read by the parent process later
     * close anyfiles we opened
     * */
    fprintf(tmp,"%i\n",line_count);
    fclose(fp);
    fclose(tmp);
    return 0;
}

int execute(char *file, int processes, unsigned int start_bytes[], unsigned int byte_offset, unsigned int size) {
        /*
         * VARIABLE DECLARATION
         * i is used for loops
         * buffer is used to store data from our temp file
         * total size stores the total number of lines
         * template allows creation of our temp file
         * fd is used to access our temp file
         * we malloc space to our array of pids
         * tmp is used for accessing our temp file
         * */
        int i;
        char buffer[32];
        unsigned int total_size = 0;
        char template[] = "/tmp/XXXXXX";
        int fd;
        fd = mkstemp(template);
        pid_t *pid;
        pid = (pid_t * )(malloc(processes * sizeof(pid_t)));
        FILE *tmp;

        /* create a temp file with append attribute, exit if error and free */
        if ((tmp = fdopen(fd, "a+")) == NULL) {
            fprintf(stderr,"error creating temp file!\n");
            free(pid);
            free(start_bytes);
            // fclose(fd);
            exit(1);
        }


        /*fork and run our command in a for loop
         * the number of times we run is the number of processes the user specified
         * */
        for (i = 0; i < processes; i++) {
            if ((pid[i] = fork()) < 0) {
                fprintf(stderr,"fork error!\n");
                /*action taken by the child*/
            } else if (pid[i] == 0) {

                /* if our last process, we need to deal with the rest of the file */
                if (processes - i == 1) {
                    byte_offset = size - (byte_offset * (processes - 1));
                }
                /* count actually counts the lines and stores it in our temp file for adding
                 * we free our resources from malloc
                 * we exit the child*/
                count(start_bytes[i], byte_offset, file, tmp);
                free(pid);
                free(start_bytes);
                exit(0);
            }

        }
        /* wait for all the children */
        while (processes > 0) {
            if (waitpid(-1, NULL, 0) < 0) {
                fprintf(stderr,"wait error");
            }

            processes--;
        }
        /*free the pid
         *
         * fseek rewinds the file to the beginning
         * */
        free(pid);
        fseek(tmp, 0, SEEK_SET);

        /* while we read in fromm our temp file
         * add to our total size
         * print the total size after the while loop and close our file
         * */
        while (fgets(buffer, sizeof(buffer), tmp) != NULL) {
            total_size += atoi(buffer);
        }
        printf("Total Line Count: %i \n", total_size);
        unlink(template);
        fclose(tmp);


        return 0;
    }



int process(unsigned int size, char *file, int processes){
    /*for use in our for loop */
    int i = 0;

    /* array to be used for storing start location */

    unsigned int loc = 0;
    unsigned int byte_offset = size/processes;

    /* allocate space in array based on how many processes we will have */
    unsigned int *start_bytes;
    start_bytes = (unsigned int *)(malloc(processes * sizeof(unsigned int)));
    /* calculate and store start location for each child process */
    for(i = 0; i < processes; i++){
        start_bytes[i] = loc;
        loc += byte_offset;
    }
    /*execute creates the children and calculates the number of lines*/
    execute(file, processes, start_bytes, byte_offset,size);
    /*we used malloc so now we must free*/
    free(start_bytes);
    return 0;
}

int main(int argc, char **argv) {
    /*used to get the size of our file */
    struct stat buf;
    if (argc != 3) {
        fprintf(stderr, "Wrong number of command-line arguments\n");
        usage();
        return 1;
    }
    /* stores the size of the file */
    unsigned int size = 0;
    /*get stats of our file, if that fails, error and quit */
    if(lstat(argv[1], &buf) < 0){
        fprintf(stderr, "error reading file! \nIs your file name valid? \n");
        usage();
        exit(0);
    }
    /* set the size received from lstat */
    size = buf.st_size;
    int processes = atoi(argv[2]);
    if(processes <= 0){
        fprintf(stderr, "Argument not a positive integer!\n");
        usage();
        return 1;
    }
    /* call our process function, which sets up for execution */
    process(size, argv[1], processes);

    return 0;
}