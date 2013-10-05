#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */

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

	for(p = j->first_process; p; p = p->next) {
	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */
	  
	  switch (pid = fork()) {

          case -1: /* fork failure */
            perror("fork");
            exit(EXIT_FAILURE);

          case 0: /* child process  */
            p->pid = getpid();	    
            new_child(j, p, fg);
              
            execvp(p->argv[0], p->argv);
            seize_tty(p->pid);
              
	    /* YOUR CODE HERE?  Child-side code for new process. */
            perror("New child should have done an exec");
            exit(EXIT_FAILURE);  /* NOT REACHED */
            break;    /* NOT REACHED */

          default: /* parent */
            /* establish child process group */
            p->pid = pid;
            set_child_pgid(j, p);
            /* YOUR CODE HERE?  Parent-side code for new process.  */
            wait((int) pid);
            
          }
            /* YOUR CODE HERE?  Parent-side code for new job.*/
	    seize_tty(getpid()); // assign the terminal back to dsh

	}
}

/* Sends SIGCONT signal to wake     up the blocked job */
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
            print_job(last_job);
            return true;
        }
        else if (!strcmp("cd", argv[0])) {
            /* Your code here */
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
    strcat(prompt_msg,"\0");
	return prompt_msg;
}

/*void redirect_input(process_t* p){
    char* fileName = p->ifile;
    int stdinput = open(j.)

    close(stdin);
    int p_argc = p->argc;
    char** p_argv = p->argv;
    int stdin = open(fileName, p_argc, p_argv);
    
}*/

int main() 
{
	init_dsh();
	DEBUG("Successfully initialized\n");

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

        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
        //if(PRINT_INFO) print_job(j);

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
            /*else{
                while(
            }*/
            j = j->next;
        }
    }
}
