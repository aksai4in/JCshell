#include <stdio.h>
#include <unistd.h>
#include "sys/wait.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

char stats[1024];
pthread_mutex_t lock;
// prints jcshell prompt
void printPrompt() {
    printf("## JCshell ## [%d] ", getpid());
    fflush(stdout);
}
// sigint handler
void handler(int sig) {
    printf("\n");
    printPrompt();
    // ignore
}
// sigint handler for child
void handlerChild(int sig) {
    // ignore
}
    
// get stats from proc files
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
// check if there is error in pipe command
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
//thread to wait for child process to go to zombie state and add stats to stats string
void *thread(void *arg)
{
    char * temp = NULL;
    siginfo_t info;
    int pid = *(int *)arg;
    if(pid != -1){
        int ret = waitid(P_PID, pid, &info, WNOWAIT | WEXITED);
        if (!ret ) {
            if (WIFEXITED(info.si_status) == 1 && WEXITSTATUS(info.si_status) == 0 || WIFSIGNALED(info.si_status) == 1) {
                temp = getstats(info.si_pid, WEXITSTATUS(info.si_status), WTERMSIG(info.si_status), info.si_status);
            }
            waitpid(pid, NULL, 0);
            if(temp != NULL){
                pthread_mutex_lock(&lock);
                strcat(stats, temp);
                pthread_mutex_unlock(&lock);
            }
        } else {
            perror("waitid");
        }
        
    }
    pthread_exit(NULL);
}
void sigusr1Handler(int signal) {
    
}

int main() {
    // signal(SIGCHLD, handleChildTermination);
    signal(SIGUSR1, sigusr1Handler);
    while(1){
        int numberOfcommands = 0;
        signal(SIGINT, handler);

        char input[1024];
        int status;
        // reset stats
        stats[0] = '\0';
        siginfo_t info;
        printPrompt();
        //create 4 pipes 
        int fd1[2];
        pipe(fd1);
        int fd2[2];
        pipe(fd2);
        int fd3[2];
        pipe(fd3);
        int fd4[2];
        pipe(fd4);
        int pid[5] = {-1, -1, -1, -1, -1};
        // get command from user
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
        char *end_str;
        char *command = strtok_r(input, "|", &end_str);
        // create arngv
        char *end_token;
        char *token = strtok_r(command, " ", &end_token);
        char *argv[10];
        int i = 0;
        while(token != NULL){
            argv[i++] = token;
            token = strtok_r(NULL, " ", &end_token);
        }
        argv[i] = NULL;
        //exit if the command is exit
        if(strcmp(argv[0], "exit") == 0){
            if(argv[1] != NULL){
                printf("JCshell: 'exit' with other arguments!!!\n");
                fflush(stdout);
                continue;
            }
            printf("JCshell: Terminated\n");
            fflush(stdout);
            break;
        }
        // go to the next command
        command = strtok_r(NULL, "|", &end_str);
        //fork
        pid[0] = fork();
        // different signal for child
        signal(SIGINT, handlerChild);
        if(pid[0] < 0){
            printf("fork failed\n");
        }else if(pid[0] == 0){
            signal(SIGUSR1, sigusr1Handler);
            // wait for sigusr1signal
            pause();
            close(fd1[0]);
            close(fd2[0]);
            close(fd2[1]);
            close(fd3[0]);
            close(fd3[1]);
            close(fd4[0]);
            close(fd4[1]);
            if(command != NULL){
                // if there is next command we need to redirect stdout to pipe
                dup2(fd1[1], 1);
            }
            close(fd1[1]);
            // execute command and print error if it fails

            execvp(argv[0], argv);
            //     char str[50];
            //     sprintf(str, "JCshell: '%s'", argv[0]);
            //     perror(str);
            //     exit(1);
            // }
        }else{
            // if there is next command, we need to fork again
        // if(command != NULL){
        //     char *end_token;
        //     char *token = strtok_r(command, " ", &end_token);
        //     char *argv[10];
        //     int i = 0;
        //     while(token != NULL){
        //         argv[i++] = token;
        //         token = strtok_r(NULL, " ", &end_token);
        //     }
        //     argv[i] = NULL;
        //     // go to the next command
        //     command = strtok_r(NULL, "|", &end_str);
        //     //fork
        //     pid[1] = fork();
        //     // different signal for child
        //     signal(SIGINT, handlerChild);
        //     if(pid[1] < 0){
        //         printf("fork failed\n");
        //     }else if(pid[1] == 0){
        //         signal(SIGUSR1, sigusr1Handler);
        //         pause();
        //         dup2(fd1[0], 0);
        //         close(fd1[0]);
        //         close(fd1[1]);
        //         close(fd2[0]);
        //         close(fd3[0]);
        //         close(fd3[1]);
        //         close(fd4[0]);
        //         close(fd4[1]);
                
        //         if(command != NULL){
        //         //     // if there is next command we need to redirect stdout to pipe
        //             dup2(fd2[1], 1);
        //         //     close(fd2[0]);
        //         //     close(fd2[1]);
        //         }
        //         close(fd2[1]);
        //         // execute command and print error if it fails
        //         if (execvp(argv[0], argv) == -1) {
        //             char str[50];
        //             sprintf(str, "JCshell: '%s'", argv[0]);
        //             perror(str);
        //             exit(1);
        //         }else{
        //             exit(0);
        //         }
        //     }
        // }
        // // if there is next command, we need to fork again
        // if(command != NULL){
        //     char *end_token;
        //     char *token = strtok_r(command, " ", &end_token);
        //     char *argv[10];
        //     int i = 0;
        //     while(token != NULL){
        //         argv[i++] = token;
        //         token = strtok_r(NULL, " ", &end_token);
        //     }
        //     argv[i] = NULL;
        //     // go to the next command
        //     command = strtok_r(NULL, "|", &end_str);
        //     //fork
        //     pid[2] = fork();
        //     // different signal for child
        //     signal(SIGINT, handlerChild);
        //     if(pid[2] < 0){
        //         printf("fork failed\n");
        //     }else if(pid[2] == 0){
        //         signal(SIGUSR1, sigusr1Handler);
        //         pause();
        //         dup2(fd2[0], 0);
        //         close(fd1[0]);
        //         close(fd1[1]);
        //         close(fd2[0]);
        //         close(fd2[1]);
        //         close(fd3[0]);
        //         close(fd4[0]);
        //         close(fd4[1]);
        //         if(command != NULL){
        //         //     // if there is next command we need to redirect stdout to pipe
        //             dup2(fd3[1], 1);
        //         //     close(fd2[0]);
        //         //     close(fd2[1]);
        //         }
        //         close(fd3[1]);
        //         // execute command and print error if it fails
        //         if (execvp(argv[0], argv) == -1) {
        //             char str[50];
        //             sprintf(str, "JCshell: '%s'", argv[0]);
        //             perror(str);
        //             exit(1);
        //         }else{
        //             exit(0);
        //         }
        //     }
        // }
        // // if there is next command, we need to fork again
        // if(command != NULL){
            
        //     char *end_token;
        //     char *token = strtok_r(command, " ", &end_token);
        //     char *argv[10];
        //     int i = 0;
        //     while(token != NULL){
        //         argv[i++] = token;
        //         token = strtok_r(NULL, " ", &end_token);
        //     }
        //     argv[i] = NULL;
        //     // go to the next command
        //     command = strtok_r(NULL, "|", &end_str);
        //     //fork
        //     pid[3] = fork();
        //     // different signal for child
        //     signal(SIGINT, handlerChild);
        //     if(pid[3] < 0){
        //         printf("fork failed\n");
        //     }else if(pid[3] == 0){
        //         signal(SIGUSR1, sigusr1Handler);
        //         pause();
        //         dup2(fd3[0], 0);
        //         close(fd1[0]);
        //         close(fd1[1]);
        //         close(fd2[0]);
        //         close(fd2[1]);
        //         close(fd3[0]);
        //         close(fd3[1]);
        //         close(fd4[0]);
        //         if(command != NULL){
        //         //     // if there is next command we need to redirect stdout to pipe
        //             dup2(fd4[1], 1);
        //         //     close(fd2[0]);
        //         //     close(fd2[1]);
        //         }
        //         close(fd4[1]);
        //         // execute command and print error if it fails
        //         if (execvp(argv[0], argv) == -1) {
        //             char str[50];
        //             sprintf(str, "JCshell: '%s'", argv[0]);
        //             perror(str);
        //             exit(1);
        //         }else{
        //             exit(0);
        //         }
        //     }
        // }
        // // if there is next command, we need to fork again
        // if(command != NULL){
        //     char *end_token;
        //     char *token = strtok_r(command, " ", &end_token);
        //     char *argv[10];
        //     int i = 0;
        //     while(token != NULL){
        //         argv[i++] = token;
        //         token = strtok_r(NULL, " ", &end_token);
        //     }
        //     argv[i] = NULL;
        //     // go to the next command
        //     command = strtok_r(NULL, "|", &end_str);
        //     //fork
        //     pid[4] = fork();
        //     // different signal for child
        //     signal(SIGINT, handlerChild);
        //     if(pid[4] < 0){
        //         printf("fork failed\n");
        //     }else if(pid[4] == 0){
        //         signal(SIGUSR1, sigusr1Handler);
        //         pause();
        //         dup2(fd4[0], 0);
        //         close(fd1[0]);
        //         close(fd1[1]);
        //         close(fd2[0]);
        //         close(fd2[1]);
        //         close(fd3[0]);
        //         close(fd3[1]);
        //         close(fd4[0]);
        //         close(fd4[1]);
        //         // execute command and print error if it fails
        //         if (execvp(argv[0], argv) == -1) {
        //             char str[50];
        //             sprintf(str, "JCshell: '%s'", argv[0]);
        //             perror(str);
        //             exit(1);
        //         }else{
        //             exit(0);
        //         }
        //     }
        // }
        //close all unused pipes
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd3[0]);
        close(fd3[1]);
        close(fd4[0]);
        close(fd4[1]);

        // sending SIGUSR1 signal to all children

        for(int i = 0; i < 5; i++){
            if(pid[i] != -1){
                numberOfcommands++;
            }
        }
        for (int i = 0; i < 1; i++) {
            kill(pid[i], SIGUSR1);
        }
        printf("JCshell: %d commands in pipe\n", numberOfcommands);
        for(int i = 0; i < 1; i++){
            char * temp = NULL;
            siginfo_t info;
            int ret = waitid(P_ALL, 0, &info, WNOWAIT | WEXITED);
            if (!ret ) {
                if (WIFEXITED(info.si_status) == 1 && WEXITSTATUS(info.si_status) == 0 || WIFSIGNALED(info.si_status) == 1) {
                    temp = getstats(info.si_pid, WEXITSTATUS(info.si_status), WTERMSIG(info.si_status), info.si_status);
                }
                waitpid(info.si_pid, NULL, 0);
                if(temp != NULL){
                    strcat(stats, temp);
                }
            } else {
                perror("waitid");
            }
        }
        // create 5 threads for waiting for child process to go to zombie state
        // pthread_t threads[5];
        // // initialize mutex lock
        // pthread_mutex_init(&lock, NULL);

        // for(int i = 0; i < 5; i ++){
        //         pthread_create(&threads[i], NULL, thread, &pid[i]);
        // }

        
        
        // for(int i = 0; i < 5; i ++){
        //     pthread_join(threads[i], NULL);
        // }
        // wait(NULL);
        // // once everything is done, print stats
        printf("%s", stats);    
        // pthread_mutex_destroy(&lock);
    }
        }
    return 0;    
}
