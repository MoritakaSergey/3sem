/*
  Write a program which allow you to: 
- run another programs via command line.
- get exit codes of terminated programs

## TIPS:
1. Use "2_fork_wait_exit.c" and "4_exec_dir.c" from examples. Combine them.
2. Parse input string according to the type of exec* (see "man exec").
   a) if execvp is used, string splitting into "path" and "args" is all you need.
3. Collect exit codes via waitpid/WEXITSTATUS.
*/
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>

#define STARTBUFFERSIZE 4 

char* getArg(char* input) 
{
	int i = 0;
	int s = 0;
	int e = 0;
	char* arg;
	while (input[s] == ' ') {
		s++;
	}
	e = s;
	while (input[e] != ' ' && input[e] != '\0') {
		e++;
	}
	arg = (char*)malloc((e - s + 1)*sizeof(char));
	i = e;
	arg[i - s] = '\0';	
	while (--i >= s)
		arg[i - s] = input[i];
	s = 0;
	while (input[e] != '\0') {
		input[s++] = input[e++];
	}
	input[s] = '\0';
	return arg;	
}


int readArgs(char** buffer)
{
	int n = 0;
	int i = 0;
	char isEmpty = 1;
	int bufferSize = STARTBUFFERSIZE;
	*buffer = (char*)malloc(bufferSize * sizeof(char));
	while (((*buffer)[i] = getchar()) != '\0') {		
		if((*buffer)[i] == '\n') {
			(*buffer)[i] = '\0';
			break;
		}
		if ((*buffer)[i] == ' ') {
			isEmpty = 1;
		} else {
			if (isEmpty == 1) {
				isEmpty == 0;
				n++;
			} 
		}
		i++;
		if (i == bufferSize) {
			bufferSize *= 2;
			*buffer = (char*)realloc(*buffer, bufferSize);
		}
	}
	return n;
}

char** separateArgs(int n, char* buffer)
{
	int i;
	char** args;
	args = (char**)malloc((n+1)*sizeof(char*));
	for (i = 0; i < n; i++) {
		args[i] = getArg(buffer);
	}
	args[n] = NULL; 
	return args;
}


void freeList(int n, char** list)
{
	int i;
	for (i = 0; i < n; i++) {
		free(list[i]);
	}
	free(list);
	return;
}

int main() 
{
	char isExecute = 1;
	int status;
	int n;
	char* buffer;
	char** argsList;
	pid_t pid = 0;
	while (isExecute) {
		pid = fork();	
		if (pid < 0) {
			printf("fork() error\n");
			return -1;
		}
		if (pid) {
			waitpid(pid, &status, 0);
			printf("Return code: %d\nPress \"q\" to finish, \"<Enter>\" to continue\n", WEXITSTATUS(status));
			if (getchar() == 'q')
				isExecute = 0;
		} else {
			n = readArgs(&buffer);
			argsList = separateArgs(n, buffer);		
			execvp(argsList[0], argsList);
		}
	}	
	return 0;
}

