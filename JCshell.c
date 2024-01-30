// Kyshtobaev Aidar 3035918139
// I ipmplemented all the features. The only thing that might be improved is the waiting time. I use sleep(1)
// in my parent process so that all the children can have enough time to go to paused state, before I send
// SIGUSR1 signal to them. I would love to get some comment's on how to make that more efficient.

#include <stdio.h>
#include <unistd.h>
#include "sys/wait.h"
#include <stdlib.h>
#include <string.h>
//return stats of the process
char * getstats(int pid,int excode, int exsig, int status){
    char str[50];
    char comm[50];
    FILE * file;
    int foo_int; 
	unsigned long long i, x;
	unsigned long h, ut, st;

    int pid_value;
    char cmd[256];
    char state;
    int ppid;
    int vctx;
    int nvctx;
    
    sprintf(str, "/proc/%d/stat", (int)pid);

    file = fopen(str, "r");
    if (file == NULL) {
		printf("Error in open my proc file\n");
		exit(0);
	}
    fscanf(file, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu", 
    &pid_value, cmd, &state, &ppid, &foo_int, &foo_int, &foo_int, &foo_int,
		(unsigned *)&foo_int, &h, &h, &h, &h, &ut, &st);

	fclose(file);

    sprintf(str, "/proc/%d/status", (int)pid);
    file = fopen(str, "r");
    if (file == NULL) {
		printf("Error in open my proc file\n");
		exit(0);
	}
    int v = -1;
    int n = -1;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "voluntary_ctxt_switches: %d", &v) == 1) {
            vctx = v;
        } else if (sscanf(line, "nonvoluntary_ctxt_switches: %d", &n) == 1) {
            nvctx = n;
        }
    }

    fclose(file);
    char * ret = malloc(sizeof(char) * 512);
    if(exsig != 0){
        sprintf(ret, "(PID)%d (CMD)%s (STATE)%c (EXSIG)%s (PPID)%d (USER)%.2lf (SYS)%.2lf (VCTX)%d (NVCTX)%d\n",
        pid_value, cmd, state, strsignal(exsig), ppid, ut*1.0f/sysconf(_SC_CLK_TCK), st*1.0f/sysconf(_SC_CLK_TCK), vctx, nvctx);
    }
    else{
        sprintf(ret, "(PID)%d (CMD)%s (STATE)%c (EXCODE)%d (PPID)%d (USER)%.2lf (SYS)%.2lf (VCTX)%d (NVCTX)%d\n",
        pid_value, cmd, state, excode, ppid, ut*1.0f/sysconf(_SC_CLK_TCK), st*1.0f/sysconf(_SC_CLK_TCK), vctx, nvctx );
    }
    return ret;

}
//sigusr1 handler 
void sigusr1Handler(int signal) {
}
//check if input has incorrect pipes
int wrongpipes(char *str){
    char * pointer = str;
    int count = -1;
    int pipeCount = 0;
    while (*pointer != '\0'){
        if(*pointer == '|'){
            pipeCount++;
            if(count == -1){
                return 2;
            }
            count++;
        }
        else if(*pointer != ' ' && *pointer != '\n'){
            count = 0;
        }
        if(count == 2 ){
            return 1;
        }
        pointer++;
    }
    if(count == 1){
        return 2;
    }
    if(pipeCount > 4){
        return 3;
    }
    return 0;
}
//check if the command is exit
int checkExit(char **argv){
    if(strcmp(argv[0], "exit") == 0){
        if(argv[1] != NULL){
            printf("JCshell: 'exit' with other arguments!!!\n");
            fflush(stdout);
            return 1;
        }
        printf("JCshell: Terminated\n");
        fflush(stdout);
        return 0;
    }
    return 2;
}
//handler for sigint after running commands
void handlerChild(int sig) {
    // ignore
}
//print prompt
void printPrompt(){
    printf("## JCshell ## [%d] ", getpid());
    fflush(stdout);
}
//handler for sigint before running commands
void handler(int sig) {
    printf("\n");
    printPrompt();
    // ignore
}

int main() {
    //declare variables
    char stats[1024] = "";
    char input[1024];
    char *end_str;
    char *command;
    char *token;
    char *end_token;
    char *argv[30];
    pid_t pid[5] = {-1, -1, -1, -1, -1};
    int counter = 0;
    signal(SIGUSR1, sigusr1Handler);
    while(1){
        signal(SIGINT, handler);
        stats[0] = '\0';
        counter = 0;
        //pipe variables
        int fd[2];
        int fd2[2];
        int fd3[2];
        int fd4[2];

        // Create pipe
        if (pipe(fd) == -1) {
            perror("pipe");
            return -1;
        }
        if (pipe(fd2) == -1) {
            perror("pipe");
            return -1;
        }
        if (pipe(fd3) == -1) {
            perror("pipe");
            return -1;
        }
        if(pipe(fd4) == -1) {
            perror("pipe");
            return -1;
        }
        printPrompt();
        //get input
        fgets(input, sizeof(input), stdin);
        input[strlen(input) - 1] = '\0';
        if(wrongpipes(input) == 1){
            printf("JCshell: should not have two | symbols without in-between command\n");
            fflush(stdout);
            continue;
        }else if(wrongpipes(input) == 2){
            printf("JCshell: should not have | symbol at the end or in the beginning\n");
            fflush(stdout);
            continue;
        }else if(wrongpipes(input) == 3){
            printf("JCshell: should not have more than 5 commands\n");
            fflush(stdout);
            continue;
        }


        //get command
        command = strtok_r(input, "|", &end_str);

        // First fork
        if(command != NULL){
            //get arguments
            token = strtok_r(command, " ", &end_token);
            int i = 0;
            while(token != NULL){
                argv[i++] = token;
                token = strtok_r(NULL, " ", &end_token);
            }
            argv[i] = NULL;
            int ex = checkExit(argv);
            if(ex == 0){
                break;
            }else if(ex == 1){
                continue;
            }

            // increase counter of children
            counter ++;
            command = strtok_r(NULL, "|", &end_str);


            //fork child
            pid[0] = fork();
            signal(SIGINT, handlerChild);
            if (pid[0] < 0) {
                perror("fork");
                return -1;
            } else if (pid[0] == 0) {
                // Child 1: executing ls command
                signal(SIGUSR1, sigusr1Handler);
                pause();
                // stdout will write to the write part of the pipe
                if(command != NULL){
                    dup2(fd[1], 1);
                }else{
                    close(fd[1]);
                }
                
                // Child does not need read end, close it
                close(fd[0]);
                close(fd2[0]);
                close(fd2[1]);
                close(fd3[0]);
                close(fd3[1]);
                close(fd4[0]);
                close(fd4[1]);
                
                if(execvp(argv[0], argv) == -1){
                    char str[50];
                    sprintf(str, "JCshell: '%s'", argv[0]);
                    perror(str);
                    exit(1);
                };
                exit(1);
            }
        }

        // Second fork
        if(command != NULL){
            //get arguments
            token = strtok_r(command, " ", &end_token);
            int i = 0;
            while(token != NULL){
                argv[i++] = token;
                token = strtok_r(NULL, " ", &end_token);
            }
            argv[i] = NULL;

            // increase counter of children
            counter ++;
            command = strtok_r(NULL, "|", &end_str);

            //fork child
            pid[1] = fork();

            if (pid[1] < 0) {
                perror("fork");
                return -1;
            } else if (pid[1] == 0) {
                signal(SIGUSR1, sigusr1Handler);
                pause();
                // Child 2: executing wc command

                
                // stdin will read from the read part of the pipe
                dup2(fd[0], 0);

                // stdout will write to the write part of the pipe
                if(command != NULL){
                    dup2(fd2[1], 1);
                }else{
                    close(fd2[1]);
                }

                
                // Child does not need write end, close it
                close(fd[1]);
                close(fd2[0]);
                close(fd3[0]);  
                close(fd3[1]);
                close(fd4[0]);
                close(fd4[1]);

                
                if(execvp(argv[0], argv) == -1){
                    char str[50];
                    sprintf(str, "JCshell: '%s'", argv[0]);
                    perror(str);
                    exit(1);
                };
                exit(1);
            }
        }
        // Third fork
        if(command != NULL){
            //get arguments
            token = strtok_r(command, " ", &end_token);
            int i = 0;
            while(token != NULL){
                argv[i++] = token;
                token = strtok_r(NULL, " ", &end_token);
            }
            argv[i] = NULL;

            // increase counter of children
            counter ++;
            command = strtok_r(NULL, "|", &end_str);

            //fork child
            pid[2] = fork();

            if (pid[2] < 0) {
                perror("fork");
                return -1;
            } else if (pid[2] == 0) {
                signal(SIGUSR1, sigusr1Handler);
                pause();
                // Child 2: executing wc command


                // stdin will read from the read part of the pipe
                dup2(fd2[0], 0);

                // stdout will write to the write part of the pipe
                if(command != NULL){
                    dup2(fd3[1], 1);
                }else{
                    close(fd3[1]);
                }
                
                // Child does not need write end, close it
                close(fd[1]);
                close(fd[0]);
                close(fd2[1]);
                close(fd3[0]);
                close(fd4[0]);
                close(fd4[1]);
                
                if(execvp(argv[0], argv) == -1){
                    char str[50];
                    sprintf(str, "JCshell: '%s'", argv[0]);
                    perror(str);
                    exit(1);
                };
                exit(1);
            }   
        }
        // Fourth fork
        if(command != NULL){
            //get arguments
            token = strtok_r(command, " ", &end_token);
            int i = 0;
            while(token != NULL){
                argv[i++] = token;
                token = strtok_r(NULL, " ", &end_token);
            }
            argv[i] = NULL;

            // increase counter of children
            counter ++;
            command = strtok_r(NULL, "|", &end_str);

            //fork child
            pid[3] = fork();

            if (pid[3] < 0) {
                perror("fork");
                return -1;
            } else if (pid[3] == 0) {
                signal(SIGUSR1, sigusr1Handler);
                pause();
                // Child 2: executing wc command


                // stdin will read from the read part of the pipe
                dup2(fd3[0], 0);

                // stdout will write to the write part of the pipe
                if(command != NULL){
                    dup2(fd4[1], 1);
                }else{
                    close(fd4[1]);
                }

                
                // Child does not need write end, close it
                close(fd[1]);
                close(fd[0]);
                close(fd2[1]);
                close(fd2[0]);
                close(fd3[1]);
                close(fd4[0]);
                
                if(execvp(argv[0], argv) == -1){
                    char str[50];
                    sprintf(str, "JCshell: '%s'", argv[0]);
                    perror(str);
                    exit(1);
                };
                exit(1);
            }
        }
        
        if(command != NULL){
            //get arguments
            token = strtok_r(command, " ", &end_token);
            int i = 0;
            while(token != NULL){
                argv[i++] = token;
                token = strtok_r(NULL, " ", &end_token);
            }
            argv[i] = NULL;

            // increase counter of children
            counter ++;
            command = strtok_r(NULL, "|", &end_str);

            //fork child
            pid[4] = fork();

            if (pid[4] < 0) {
                perror("fork");
                return -1;
            } else if (pid[4] == 0) {
                signal(SIGUSR1, sigusr1Handler);
                pause();
                // Child 2: executing wc command


                // stdin will read from the read part of the pipe
                dup2(fd4[0], 0);

                
                // Child does not need write end, close it
                close(fd[1]);
                close(fd[0]);
                close(fd2[1]);
                close(fd2[0]);
                close(fd3[1]);
                close(fd3[0]);
                close(fd4[1]);
                
                if(execvp(argv[0], argv) == -1){
                    char str[50];
                    sprintf(str, "JCshell: '%s'", argv[0]);
                    perror(str);
                    exit(1);
                };
                exit(1);
            }
        }
        

        // Parent process
        sleep(1);
        for(int i = 0; i < counter; i ++){
            kill(pid[i], SIGUSR1);
        }
        // Parent does not need any ends of the pipe, close them
        close(fd[0]);
        close(fd[1]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd3[0]);
        close(fd3[1]);
        close(fd4[0]);
        close(fd4[1]);
        
        // Wait for all children to finish

        for(int i = 0; i < counter; i++){
            siginfo_t info;
            int ret = waitid(P_ALL, 0, &info, WNOWAIT | WEXITED);
            if (!ret ) {
                if (WIFEXITED(info.si_status) == 1 && WEXITSTATUS(info.si_status) == 0 || WIFSIGNALED(info.si_status) == 1) {
                    strcat(stats, getstats(info.si_pid, WEXITSTATUS(info.si_status), WTERMSIG(info.si_status), info.si_status));
                }
                waitpid(info.si_pid, NULL, 0);   
            } else {
                perror("waitid");
            }
        }
        printf("%s", stats);

    }

    return 0;
}