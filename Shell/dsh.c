#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */
job_t *first_job;

/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
    if (j->pgid < 0) /* first child: use its pid for job pgid */
        j->pgid = p->pid;
    return(setpgid(p->pid,j->pgid));
}

/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
    /* establish a new process group, and put the child in
     * foreground if requested
     */
    
    /* Put the process into the process group and give the process
     * group the terminal, if appropriate.  This has to be done both by
     * the dsh and in the individual child processes because of
     * potential race conditions.
     * */
    
    p->pid = getpid();
    
    /* also establish child process group in child to avoid race (if parent has not done it yet). */
    set_child_pgid(j, p);
    
    if(fg) // if fg is set
		seize_tty(j->pgid); // assign the terminal
    
    /* Set the handling for job control signals back to the default. */
    signal(SIGTTOU, SIG_DFL);
}

/*test change*/
/* Spawning a process with job control. fg is true if the
 * newly-created process is to be placed in the foreground.
 * (This implicitly puts the calling process in the background,
 * so watch out for tty I/O after doing this.) pgid is -1 to
 * create a new job, in which case the returned pid is also the
 * pgid of the new job.  Else pgid specifies an existing job's
 * pgid: this feature is used to start the second or
 * subsequent processes in a pipeline.
 * */

void spawn_job(job_t *j, bool fg)
{
	pid_t pid;
	process_t *p;
    bool prev_cmd_exists = false;
    int fd[2] = {0,0};
    int fdcopy[2] = {0,0};
	for(p = j->first_process; p; p = p->next) {
        /* YOUR CODE HERE? */
        /* Builtin commands are already taken care earlier */
        int input_fd = -1; //input_fd will have value -1 upon completion of switch if there is no input file
        int output_fd = -1; //similar as input_fd but for output
        /*if there is an ofile, then it cannot pipe to the next process, since the process will always write everything
         to the outfile. Additionally, if there is no next process, then it cannot pipe*/
        if(p->ofile == NULL && p->next != NULL){
            fdcopy[0] = fd[0];
            fdcopy[1] = fd[1];
            pipe(fd);
            dup2(STDOUT_FILENO, fd[1]);
            //dup2(fd[1],0);
        }

        switch (pid = fork()) {
            
            case -1: /* fork failure */
                perror("fork");
                exit(EXIT_FAILURE);
            case 0: /* child process  */
                p->pid = getpid();
                new_child(j, p, fg);
                //seize_tty(p->pid);
                if(endswith(p->argv[0], ".c")){
                    char** args = malloc(3);
                    args[0] = "-o";
                    args[1] = "devil";
                    char* filename = "";
                    strcat(filename, "/");
                    strcat(filename, p->argv[0]);
                    args[2] = filename;
                    p->argv[0] = "devil";
                    execvp("/usr/bin/gcc", args);
                }
                if(p->ifile!=NULL){
                    input_fd = redirect_input(p);
                }
                if(p->ofile!=NULL){
                    output_fd = redirect_output(p);
                }
                if(p->ifile == NULL && prev_cmd_exists){
                    dup2(fdcopy[0],STDIN_FILENO);
                }
                prev_cmd_exists = true;
                execvp(p->argv[0], p->argv);
                
                /* YOUR CODE HERE?  Child-side code for new process. */
                perror("New child should have done an exec");
                exit(EXIT_FAILURE);  /* NOT REACHED */
                p->completed = false;
                break;    /* NOT REACHED */
                
            default: /* parent */
                /* establish child process group */
                p->pid = pid;
                printf("%d\n", pid);
                set_child_pgid(j, p);
                
                /* YOUR CODE HERE?  Parent-side code for new process.  */
                wait(&pid);
                if(input_fd!=-1){
                    dup2(0,input_fd); //redirects input back to what it was before switches
                }
                if(output_fd!=-1){
                    dup2(1,output_fd); //redirects output back to what it was before switches
                }
                
                p->completed = true;
                p->stopped = false;
        }
        /* YOUR CODE HERE?  Parent-side code for new job.*/
	    seize_tty(getpid()); // assign the terminal back to dsh
        
	}
}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j)
{
    if(kill(-j->pgid, SIGCONT) < 0)
        perror("kill(SIGCONT)");
}


/*
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.
 */

bool fg(job_t* j){
    return true;
}

bool builtin_cmd(job_t *last_job, int argc, char **argv)
{
    
    /* check whether the cmd is a built in command
     */
    
    if (!strcmp(argv[0], "quit")) {
        /* Your code here */
        stdin = 0;
        exit(EXIT_SUCCESS);
    }
    else if (!strcmp("jobs", argv[0])) {
        jobs(first_job);
        return true;
    }
    else if (!strcmp("cd", argv[0])) {
        chdir(argv[1]);
        last_job->first_process->completed = true;
        return true;
    }
    else if (!strcmp("bg", argv[0])) {
        return true;
    }
    else if (!strcmp("fg", argv[0])) {
        return true;
    }
    return false;       /* not a builtin command */
}

bool builtin(job_t* j){
    if(!strcmp(j->commandinfo, "jobs") || !strcmp(j->commandinfo, "cd") || !strcmp(j->commandinfo, "bg") || !strcmp(j->commandinfo, "fg")){
        return true;
    }
    return false;
}

void jobs(job_t* j){
    while(j!=NULL){
        if(builtin(j) || j->notified){
            j=j->next;
            continue;
        }
        if(job_is_completed(j)){
            printf("%d (completed): ", j->pgid);
            printf("%s\n", j->commandinfo);
            j->notified = true;
        }
        else if(job_is_stopped(j)){
            printf("%d (stopped): ", j->pgid);
            printf("%s\n", j->commandinfo);
        }
        else{
            printf("%d (not completed): ", j->pgid);
            printf("%s\n", j->commandinfo);
        }
        j = j->next;
    }
}
/* Build prompt messaage */
char* promptmsg()
{
    char *pid_string = malloc(20);
    int pid = (int) getpid();
    sprintf(pid_string, "%d", pid);
    char* prompt_msg = malloc(40);
    strcpy(prompt_msg, "dsh-");
    strcat(prompt_msg, pid_string);
    strcat(prompt_msg, "$");
    strcat(prompt_msg, " ");
    strcat(prompt_msg,"\0");
	return prompt_msg;
}


int redirect_input(process_t* p){
    char* fileName = p->ifile;
    int fd0 = open(fileName, O_RDONLY);
    dup2(fd0, 0);
    return fd0;
}

int redirect_output(process_t* p){
    char* fileName = p->ofile;
    int fd0 = creat(fileName, S_IREAD | S_IWRITE);
    dup2(fd0, 1);
    return fd0;
}

int main()
{
	init_dsh();
	DEBUG("Successfully initialized\n");
    bool first = true;
	while(1) {
        job_t *j = NULL;
		if(!(j = readcmdline(promptmsg()))) {
			if (feof(stdin)) { /* End of file (ctrl-d) */
				fflush(stdout);
				printf("\n");
				exit(EXIT_SUCCESS);
            }
			continue; /* NOOP; user entered return or spaces with return */
		}
        if(!first){
            job_t *lastjob = find_last_job(first_job);
            lastjob->next = j;
        }
        else{
            first_job = j;
            first = false;
        }
        //print_job(first_job);
        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
        /* Your code goes here */
        /* You need to loop through jobs list since a command line can contain ;*/
        while(j!=NULL){
            if(!builtin_cmd(j, j->first_process->argc, j->first_process->argv)){
                if(fg(j)){
                    spawn_job(j, true);
                }
                else{
                    spawn_job(j, false);
                }
            }
            j = j->next;
        }
    }
}
