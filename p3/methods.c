/*	CS 325 Program 3:  Function Implementations
*	Programmed by: Kelvin Watson
*	File name: methods.c
*	Date created: 7 Nov 2015
* 	Last modified: 7 Nov 2015
*  	Citations: http://stackoverflow.com/questions/2605130/redirecting-exec-output-to-a-buffer-or-file
* 	http://pubs.opengroup.org/onlinepubs/009695399/functions/exec.html
	http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
	http://beej.us/guide/bgipc/output/html/multipage/signals.html
*/ 

#include "methods.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/*
 * Global variable to store current foreground process 
 */
static pid_t fgp; //since only one fg process can be active at once, it's OK to create global variable

/*
 * Prints title 
 */
void printTitle(){
	printf("\n------------------------------------------------------------"); FLUSH;
	printf("\nSmall Shell\tProgrammed by: Kelvin Watson"); FLUSH;
	printf("\n------------------------------------------------------------\n"); FLUSH;
}

/*
 * Clears buffer after keyboard input via fgets 
 */
void clearBuffer(FILE* fp){
	int c;    
	while ( (c = fgetc(fp)) != EOF && c != '\n');
}

/* 
 * SIGCHLD handler that cleans up the child process  
 */ 
void cleanBG(int signo){
	int status;
	pid_t cp;
	do{
		cp=waitpid(-1, &status, WNOHANG);
		if(cp>0){
			if(WIFEXITED(status)){ //check if bg process exited normally
				printf("background pid %d is done: exit value %d\n",cp,WEXITSTATUS(status)); FLUSH;
			}
			else if(WIFSIGNALED(status)){
				printf("background pid %d is done: terminated by %d\n",cp,WTERMSIG(status)); FLUSH;
			}	
		}				
	}while(cp != (pid_t)-1 && cp !=(pid_t)0);
	//ignore SIGINT: do not allow signo==SIGINT to terminate any bg process
} 

/* 
 * SIGTERM handler that kills foreground(child) process and ignores SIGINT in parent
 */ 
void terminateFG(int signo){
	printf("terminated by signal %d\n", signo); FLUSH; //parent ignores SIGINT
	kill(fgp,signo); //terminates child
}

/*
 * Executes commands with arguments with no IO redirection
 */
int executeCommand(char** argv, int len, int bg, pid_t* chld){
	int fdo;
	#if(TRACE)
	printf("TRACE: In executeCommand()\n"); FLUSH;
	#endif
	int childStatus;
	if(bg){
		#if(TRACE)
		printf("TRACE: Last word=%s, Run in background!\n",argv[len-2]); FLUSH;//run process in background 
		#endif
		//Source: http://www.makelinux.net/alp/026
		struct sigaction bgSA; 
		memset (&bgSA, 0, sizeof(bgSA)); 
		bgSA.sa_handler = &cleanBG; 
		//bgSA.sa_flags = SA_RESTART | SA_NOCLDWAIT | SA_NOCLDSTOP;
		//http://beej.us/guide/bgipc/output/html/multipage/signals.html (to prevent interrupting fgets() system call)
		//bgSA.sa_flags = SA_RESTART; 
		bgSA.sa_flags = SA_RESTART;
		sigfillset(&(bgSA.sa_mask));
		if(sigaction(SIGCHLD,&bgSA,0) == -1) {
			perror("SIGCHLD");
			exit(1);
		}		
		if(sigaction(SIGINT,&bgSA,0) == -1) {
			perror("SIGINT");
			exit(1);
		}
		if(sigaction(SIGTERM,&bgSA,0) == -1){
			perror("SIGTERM");
			exit(1);			
		}
		FLUSH; //macro for fflush(stdout). Perform flush before fork
		
		/* Signal handler registered, stdout flushed, OK to fork()  */		
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
			case -1:{ //fork failure
				perror("\nTRACE: Fork unsuccessful. Exiting...\n"); FLUSH;
				exit(1);
			}
			case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: I'm the child, getpid()=%d!\n",getpid()); FLUSH;
				#endif
				
				signal(SIGINT, SIG_IGN); // background processes should ignore SIGINT
					
				//clearBuffer(stdin); //clear residual content in STDIN
				fdo=open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR); //opens the file /dev/null from the root dir & obtain its descriptor
				dup2(fdo,STDOUT_FILENO); //STDOUT_FILENO==1
				close(fdo);
				if((execvp(argv[0],argv))== -1){
					perror("execvp"); FLUSH;
				} 
				break;
			}
			default:{ //parent
				//waitpid(r,&childStatus,WUNTRACED | WCONTINUED | WNOHANG); //waitpid's behavior is unpredictable if incorrect args are passed in
				(*chld)=r;
				printf("background pid is %d\n",r); FLUSH;
				return childStatus; //return child's exit status or terminating signal 
			}
		}
	} else{ //run process in foreground
		#if(TRACE)
		printf("TRACE: Run in foreground!\n"); FLUSH; //run process in background 
		#endif
		struct sigaction fgParentSA; 
		memset (&fgParentSA, 0, sizeof(fgParentSA)); 
		fgParentSA.sa_handler = &terminateFG; 
		//fgParentSA.sa_flags = SA_RESTART | SA_NOCLDWAIT | SA_NOCLDSTOP;
		fgParentSA.sa_flags = 0;
		if(sigaction(SIGINT,&fgParentSA,0)==-1){
			perror("sigaction(SIGINT)"); FLUSH;
			exit(1);			
		}
		if(sigaction(SIGTERM,&fgParentSA,0)==-1){
			perror("sigaction(SIGTERM)"); FLUSH;
			exit(1);			
		}
		FLUSH;
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
			case -1:{ //fork failure
				perror("\nTRACE: Fork unsuccessful. Exiting...\n"); FLUSH;
				exit(1);
			}
			case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: I'm the child, getpid()=%d!\n",getpid()); FLUSH;
				#endif
				
				execvp(argv[0],argv); //destroys the currently running child process!\n
				perror("execvp"); FLUSH;
				break;
			}
			default:{ //parent
				*chld = fgp = r;
				waitpid(r, &childStatus, WUNTRACED | WCONTINUED); //waitpid's behavior is unpredictable if incorrect args are passed in
				return childStatus; //return child's exit status or terminating signal 
			}
		}
	}
}

int redirectIO(char** argv, char* fileIn, char* fileOut, int len, int bg, pid_t* chld){
	#if(TRACE)
	printf("TRACE: In redirectIO()\n"); FLUSH;
	#endif
	int i, fdi, fdo;
	int childStatus;
	#if(TRACE)
	printf("\nTRACE: Printing all argv's in redirectIO():\n"); FLUSH;
	for(i=0; i<len; i++){
		printf("argv[%d]=%s\n",i,argv[i]); FLUSH;
	}
	printf("bg=%d\n",bg); FLUSH;
	#endif
	if (bg) {
		#if(TRACE)
		printf("TRACE: Last word=%s. Run in background!\n",argv[len-2]); FLUSH; //run process in background 
		#endif		
		//Source: http://www.makelinux.net/alp/026
		struct sigaction sigchld_action; 
		memset (&sigchld_action, 0, sizeof (sigchld_action)); 
		sigchld_action.sa_handler = &cleanBG; 
		sigaction (SIGCHLD, &sigchld_action, NULL); 
		/* Now ready to fork child process */ 
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
    		case -1:{ //fork failure
				#if(TRACE)
				perror("\nTRACE: Fork unsuccessful. Exiting...\n");
				#endif
				exit(1);
			}
    		case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: I'm the child!\n"); FLUSH;
				printf("TRACE: Redirecting input now...\n"); FLUSH;
				#endif
				/* REDIRECT INPUT to get input from file or /dev/null if no filename specified */
				if(!fileIn){ //if no filename supplied
					fdi=open("/dev/null", O_RDONLY); //opens the file /dev/null from the root dir & obtain its descriptor
				}
				else{ // filename provided
					fdi=open(fileIn, O_RDONLY); //open file named fileName from the curr dir and obtain its descriptor
				}
				dup2(fdi, STDIN_FILENO); //STDIN_FILENO=0, replace stdin w/input file(make stdout goto file)- STDIN_FILENO now points to same file as fd
				//dup2 closes the file descriptor for STDIN_FILENO before duplicating it onto stdin
				close(fdi); //fd no longer needed as stdin now also points to same file as fdi did (either the file or /dev/null)
				/* REDIRECT OUTPUT to /dev/null to prevent output to terminal */
				#if(TRACE)
				printf("TRACE: Redirecting output now...\n"); FLUSH;
				#endif
				fdo=open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR); //opens the file /dev/null from the root dir & obtain its descriptor
				dup2(fdo,STDOUT_FILENO); //STDOUT_FILENO=1
				close(fdo); //since stdout fd points to /dev/null now, fdo is no longer needed
				execvp(argv[0],argv); //destroys the currently running child process
				_exit(1);
				break;
			}
    		default:{ //parent
				(*chld)=r;
				waitpid(r,&childStatus, WUNTRACED); //waitpid's behavior is unpredictable if incorrect args are passed in
				return childStatus;
			}
		}
	} else{ //run process in foreground
		#if(TRACE)
		printf("TRACE: Run in foreground!\n"); FLUSH;
		#endif
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
			case -1:{ //fork failure
				perror("\nTRACE: Fork unsuccessful. Exiting...\n");
				exit(1);
			}
			case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: I'm the child!\n"); FLUSH;
				#endif
				
				/* REDIRECT INPUT to get input from file or /dev/stdin if no filename specified */
				if(fileIn){ //if filename provided, read from it
					fdi=open(fileIn, O_RDONLY);	

				}//else keep stdin as default (keyboard input) fdi=open("/dev/stdin", O_RDONLY);
				dup2(fdi, 0); //replace stdin (keyboard) w/input file(make stdout goto file)- STDIN_FILENO now points to same file as fd
				close(fdi); //fd no longer needed as stdin now also points to same file				
				
				if ((fdo=open(fileOut, O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))==-1){
					perror("open");
				} //opens the file /dev/null from the root dir & obtain its descriptor
				dup2(fdo,STDOUT_FILENO); //STDOUT_FILENO=1
				close(fdo); // fdo is no longer needed
				
				/* Execute the process, destroying the current child process */
				execvp(argv[0],argv); //destroys the currently running child process!\n
				_exit(1);
				break;
			}
			default:{ //parent
				(*chld)=r;
				waitpid(r,&childStatus, WUNTRACED); //waitpid's behavior is unpredictable if incorrect args are passed in
				return childStatus;
			}
		}
	}
}

int redirectInput(char** argv, char* fileName, int len, int bg, pid_t* chld){
	#if(TRACE)
	printf("TRACE: In redirectInput()\n");
	#endif
	int i, fdi, fdo;
	int childStatus;
	#if(TRACE)
	printf("\nTRACE: Printing all argv's in redirectInput(), fileName=%s:\n",fileName);
	for(i=0; i<len; i++){
		printf("argv[%d]=%s\n",i,argv[i]); FLUSH;
	}
	printf("bg=%d\n",bg); FLUSH;
	#endif
	if (bg) {
		#if(TRACE)
		printf("TRACE: Last word=%s. Run in background!\n",argv[len-2]); FLUSH;//run process in background 
		#endif		
		//Source: http://www.makelinux.net/alp/026
		struct sigaction sigchld_action; 
		memset (&sigchld_action, 0, sizeof (sigchld_action)); 
		sigchld_action.sa_handler = &cleanBG; 
		sigaction(SIGCHLD, &sigchld_action, NULL);
		/* Now ready to fork child process */ 
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
    		case -1:{ //fork failure
				perror("\nTRACE: Fork unsuccessful. Exiting...\n"); FLUSH;
				exit(1);
			}
    		case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: I'm the child!\n"); FLUSH;
				printf("TRACE: Redirecting input now...\n"); FLUSH;
				#endif
				/* REDIRECT INPUT to get input from file or /dev/null if no filename specified */
				if(!fileName){ //if no filename supplied
					if ( (fdi=open("/dev/null", O_RDONLY))==-1){
						perror("open");
					} //opens the file /dev/null from the root dir & obtain its descriptor
				}else{ // filename provided
					if( (fdi=open(fileName, O_RDONLY))==-1){
						perror("open");
					} //open file named fileName from the curr dir and obtain its descriptor
				}
				dup2(fdi, STDIN_FILENO); //STDIN_FILENO=0, replace stdin w/input file(make stdout goto file)- STDIN_FILENO now points to same file as fd
				//dup2 closes the file descriptor for STDIN_FILENO before duplicating it onto stdin
				close(fdi); //fd no longer needed as stdin now also points to same file as fdi did (either the file or /dev/null)
				/* REDIRECT OUTPUT to /dev/null to prevent output to terminal */
				#if(TRACE)
				printf("TRACE: Redirecting output now...\n"); FLUSH;
				#endif
				fdo=open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR); //opens the file /dev/null from the root dir & obtain its descriptor
				dup2(fdo,STDOUT_FILENO); //STDOUT_FILENO=1
				close(fdo); //since stdout fd points to /dev/null now, fdo is no longer needed
				execvp(argv[0],argv); //destroys the currently running child process
				_exit(1);
				break;
			}
    		default:{ //parent
				(*chld)=r;
				waitpid(r,&childStatus, WUNTRACED | WNOHANG); //waitpid's behavior is unpredictable if incorrect args are passed in
				return childStatus;
	        }
		}
	} else{ //run process in foreground
		#if(TRACE)
		printf("TRACE: Run in foreground!\n"); FLUSH;
		#endif
		FLUSH;
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
    		case -1:{ //fork failure
				perror("\nTRACE: Fork unsuccessful. Exiting...\n"); FLUSH;
				exit(1);
			}
    		case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: I AM HERE: FG: I'm the child!\n"); FLUSH;
				printf("This is the child process. My PID is %u and my parent's is %u",getpid(),getppid()); FLUSH;
				#endif
				/* REDIRECT INPUT to get input from file or /dev/stdin if no filename specified */
				if(fileName){ //if filename provided, try to read from it
					if( (fdi=open(fileName, O_RDONLY )) == -1){
						perror("open"); FLUSH;
						#if(TRACE)
						printf("\nerrno=%d\n",errno); FLUSH;
						#endif
						//close(fdi);
						_exit(1);
					}
					dup2(fdi, 0); //replace stdin (keyboard) w/input file(make stdout goto file)- STDIN_FILENO now points to same file as fd
					close(fdi); //fd no longer needed as stdin now also points to same file				
				}//else keep stdin as default (keyboard input) fdi=open("/dev/stdin", O_RDONLY);
				/*No need to redirect stdout here as we want the process to output to terminal*/
    				/* Execute the process, destroying the current child process */
				if( (execvp(argv[0],argv)) == -1){
					exit(EXIT_FAILURE);
				}
				_exit(1);
				break;
			}
	    	default:{ //parent
				#if(TRACE)
				printf("I'm the parent with pid %u ", getpid()); FLUSH;
				#endif
				(*chld)=r;
				waitpid(r,&childStatus, WUNTRACED); //waitpid's behavior is unpredictable if incorrect args are passed in
				return childStatus;
			}
		}
	}
}


int redirectOutput(char** argv, char* fileName,  int len, int bg, pid_t* chld){
	#if(TRACE)
	printf("TRACE: In redirectOutput()\n"); FLUSH;
	#endif
	int i, fdo;
	int childStatus;
	#if(TRACE)
	printf("\nTRACE: Printing all argv's in redirectOutput():\n"); FLUSH;
	for(i=0; i<len; i++){
		printf("argv[%d]=%s\n",i,argv[i]); FLUSH;
	}
	printf("bg=%d\n",bg); FLUSH;
	#endif
	if (bg) {
		#if(TRACE)
		printf("TRACE: Last word=%s. Run in background!\n",argv[len-2]); FLUSH;//run process in background 
		#endif		
		//Source: http://www.makelinux.net/alp/026
		struct sigaction sigchld_action; 
		memset (&sigchld_action, 0, sizeof (sigchld_action)); 
		sigchld_action.sa_handler = &cleanBG; 
		sigaction (SIGCHLD, &sigchld_action, NULL); 
		/* Now ready to fork child process */ 
		FLUSH;
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
    		case -1:{ //fork failure
				perror("\nTRACE: Fork unsuccessful. Exiting...\n"); FLUSH;
				exit(1);
			}
    		case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: I'm the child!\n"); FLUSH;
				printf("TRACE: Redirecting output now...\n"); FLUSH;
				#endif
				/* REDIRECT OUTPUT to /dev/null to prevent output to terminal */
				#if(TRACE)
				printf("TRACE: Redirecting output now...\n"); FLUSH;
				#endif
				fdo=open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR); //opens the file /dev/null from the root dir & obtain its descriptor
				dup2(fdo,STDOUT_FILENO); //STDOUT_FILENO=1
				close(fdo); //since stdout fd points to /dev/null now, fdo is no longer needed
				execvp(argv[0],argv); //destroys the currently running child process
				_exit(1);
				break;
			}
    		default:{ //parent
				(*chld)=r;
				waitpid(r,&childStatus, WUNTRACED); //waitpid's behavior is unpredictable if incorrect args are passed in
				return childStatus;
			}
		}
	} else{ //run process in foreground
		#if(TRACE)
		printf("TRACE: Run in foreground!\n");  FLUSH;
		#endif
		FLUSH;
		pid_t r = fork(); //base decision on return value of fork
		switch(r){
		case -1:{ //fork failure
				perror("\nTRACE: Fork unsuccessful. Exiting...\n"); FLUSH;
				exit(1);
			}
		case 0:{ //child
				#if(TRACE)
				printf("\nTRACE: WRITE FG: I'm the child! fileName=%s\n",fileName); FLUSH;
				#endif
				if ((fdo=open(fileName, O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))==-1){
					perror("open"); FLUSH;
				} //opens the file /dev/null from the root dir & obtain its descriptor
				dup2(fdo,STDOUT_FILENO); //STDOUT_FILENO=1
				close(fdo); // fdo is no longer needed
				/* Execute the process, destroying the current child process */
				execvp(argv[0],argv);				
				perror("execvp");FLUSH;
				_exit(1);				
				break;
			}
		default:{ //parent
				(*chld)=r;
				waitpid(r,&childStatus, WUNTRACED); //waitpid's behavior is unpredictable if incorrect args are passed in
				return childStatus;
			}
		}
	}
}
