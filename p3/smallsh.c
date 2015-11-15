/*	CS 325 Program 2: smallsh (small shell)
 *	Programmed by: Kelvin Watson
 *	File name: watsokel.adventure.c
 *	Date created: 20 Oct 2015
 * 	Last modified: 25 Oct 2015
 *	Citations: http://stackoverflow.com/questions/15472299/split-string-into-tokens-and-save-in-an-array
 *  
 */
 
#include "methods.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>


//TRACE is for testing (trace statements) only

extern sig_atomic_t childExitStatus; 

int main(){	
	int childProcesses[MAX_PROCESSES] = {-5};
	pid_t childProcessID;
	int childProcessCounter=0;
	int i, j, k;
	int numArgs;
	int childStatus=0; //last foreground process' exit status or terminating signal
	int redirectIn, redirectOut;
	int bgProcess = FALSE;
	char userInput[MAX_CMD_LENGTH];
	char userInputCopy[MAX_CMD_LENGTH];
	char* argv[MAX_ARGUMENTS]; //maximum number of arguments 
	char* token, *inputExists;
	char fileNameIn[80], fileNameOut[80];
	
	printTitle();

	/* Accept commands while command not exit */
	do{
		memset(userInput,0,sizeof(userInput)); //clear the command string
		PRINT_PROMPT(:);
		if((inputExists=(fgets(userInput, sizeof(userInput), stdin))) != NULL){
			// Determine if buffer needs to be cleared
			size_t len = strlen(userInput);
			if((userInput[len-1])=='\n'){
				userInput[len-1]='\0';
			}else{
				clearBuffer(stdin);
			}
			#if(TRACE)
			printf("TRACE: userInput(raw input)=%s\n",userInput); FLUSH;
			#endif
		}
		strcpy(userInputCopy,userInput); //save the input incase it is needed later, strtok is destructive
		
		/* Ignore empty commands */
		if(!inputExists){
			#if(TRACE)
			printf("TRACE: fgets returned null!\n"); FLUSH;
			/*Check error caused by fgets()*/
			if (feof(stdin))          /* if failure caused by end-of-file condition */
				puts("End of file reached");
			else if (ferror(stdin)){
				perror("fgets()");
				fprintf(stderr,"fgets() failed in file %s at line # %d\n", __FILE__,__LINE__-9);
				exit(EXIT_FAILURE);
			}
			#endif	
			continue;
		}
		if((int)(strlen(userInput))==0){
			#if(TRACE)
			printf("TRACE: strlen 0 !\n"); FLUSH;
			#endif
			continue;
		}
		/* Parse the input to separate the line into individual words */
		/* This handles one input and one output redirect */
		token = strtok(userInput, " ");
		#if(TRACE)
		printf("TRACE: token #1=%s\n",token); FLUSH;
		#endif
		i=0, j=0;
		redirectIn = redirectOut = FALSE;
		bgProcess = FALSE;
		do{	
			if((strncmp(token,"<",1))==0){ //detect redirect symbol,but do not include
				redirectIn=TRUE;
				token = strtok(NULL, " "); 	//retrieve next token without saving into argv
				strcpy(fileNameIn,token); 	//save the next token as it is the filename
			} else if((strncmp(token,">",1))==0){ //detect redirect symbol, but do not include 
				redirectOut=TRUE;
				token = strtok(NULL, " "); 	//retrieve next token without saving into argv
				strcpy(fileNameOut,token); 	//save the next token as it is the filename
			} else if ((strncmp(token,"&",1))==0){ //detect background symbol, but do not include
				bgProcess=TRUE;
			} else if(token[strlen(token)-1] == '$' && token[strlen(token)-2]=='$') {
				//replace token with the expanded $$
				#if(TRACE)
				printf("testDir was=%s",token);FLUSH;
				#endif
				char tmp[80]={0};
				strcpy(tmp,token);
				tmp[strlen(tmp)-1]=tmp[strlen(tmp)-2]=0; 
				sprintf(tmp+strlen(tmp),"%ld",(long)getpid());
				#if(TRACE)
				printf("testDir is now=%s",tmp);FLUSH;
				#endif
				argv[i++]=strdup(tmp);	
				
			}else{
				argv[i++]=strdup(token);
			}
			token = strtok(NULL, " ");
		}while(token!=NULL);

		memset(userInput,0,sizeof(userInput)); //clear the command string
		
		argv[i]=NULL; //last element i is null
		numArgs = i+1;
		#if(TRACE)
		printf("TRACE: i=%d,numArgs=%d",i,numArgs); FLUSH;
		#endif
		
		#if(TRACE)
		printf("\nTRACE: Printing all argv's:\n");
		for(j=0; j<i; j++){
			printf("argv[%d]=%s\n",j,argv[j]); FLUSH;
		}
		#endif		

		if(redirectIn && redirectOut){ //one input one output
			#if(TRACE)
			printf("TRACE: Both < and > detected!\n"); FLUSH;
			#endif
			childStatus = redirectIO(argv,fileNameIn,fileNameOut,numArgs,bgProcess,&childProcessID);
			childProcesses[childProcessCounter++]=childProcessID; //record child processes
		}else if (redirectIn){ //redirect stdin
			#if(TRACE)
			printf("TRACE: < detected!\n"); FLUSH;
			#endif
			childStatus = redirectInput(argv,fileNameIn,numArgs,bgProcess,&childProcessID);
			childProcesses[childProcessCounter++]=childProcessID; //record child processes
		} else if(redirectOut){ //redirect stdout
			#if(TRACE)
			printf("TRACE: > detected!\n"); FLUSH;
			#endif
			childStatus = redirectOutput(argv,fileNameOut,numArgs,bgProcess,&childProcessID);
			childProcesses[childProcessCounter++]=childProcessID; //record child processes
		}else if(userInput[0]=='#'){
			#if(TRACE)
			printf("TRACE: Comment, not a command!\n"); FLUSH;
			#endif
			continue;
		}
		else if(strcmp(argv[0],"exit")==0){			
			#if(TRACE)
			printf("\nTRACE: Killing all processes, and breaking..."); FLUSH;
			#endif	
			for(k=0; k<childProcessCounter; k++){
				#if(TRACE)
				printf("childProcessID=%d", (int)childProcesses[k]); FLUSH;
				#endif
				kill(childProcesses[k],SIGKILL);
				//send kill signal to 
			}
			//TODO: then kill myself by exiting
			exit(0);
		} else if(strcmp(argv[0],"cd")==0) {
			#if(TRACE)
			printf("TRACE: Changing directories...:\n"); FLUSH;
			#endif
			//if argv[1] == null, then change directory to HOME
			numArgs==2? chdir(getenv("HOME")) : chdir(argv[1]);	
		} else if(strcmp(argv[0],"status")==0){
			//printf("childExitStatus=%d",(int)childExitStatus);
			if(WIFEXITED(childStatus)){ //check status of child
				printf("exit value %d\n", WEXITSTATUS(childStatus)); FLUSH;
			 }else if(WIFSIGNALED(childStatus)){
				printf("terminated by signal %d\n", WTERMSIG(childStatus)); FLUSH; //unhandled signals
			 }else if(WIFSTOPPED(childStatus)){
				printf("stopped by signal %d\n",WSTOPSIG(childStatus)); FLUSH;
			}else{
				printf("exit value 1\n"); FLUSH;
			}
		} else { //all other commands
			childStatus = executeCommand(argv, numArgs, bgProcess, &childProcessID); 
			childProcesses[childProcessCounter++]=childProcessID; //record child processes
		}
		
		#if(TRACE)
		printf("freeing memory\n"); FLUSH;
		#endif
		/* Free memory */
		for(i=0; i<numArgs; i++){
			free(argv[i]);
		}
		memset(userInput,0,sizeof(userInput)); //clear the command string

	}while(1);
	return 0;
}
