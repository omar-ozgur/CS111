#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <termios.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

const int bufSize = 1000;
struct termios savedAttr;
pid_t pid;
pthread_t tid;
int shell = 0;
int status;
int pipe1[2];
int pipe2[2];

void restoreAttr(){
	if(shell){
		int shellStatus;
		waitpid(pid, &shellStatus, 0);
		if(WIFEXITED(shellStatus)){
			printf("The shell exited with status %d\n", WEXITSTATUS(shellStatus));
		}
		else if(WIFSIGNALED(shellStatus)){
			printf("The shell exited due to signal %d (%s)\n", WTERMSIG(shellStatus), strsignal(shellStatus));
		}
		else{
			printf("The shell exited normally");
		}
	}
	tcsetattr(0, TCSANOW, &savedAttr);
}

void ctrlD(){
	if(shell){
		pthread_cancel(tid);
		close(pipe2[0]);
		close(pipe1[1]);
		kill(pid, SIGHUP);
		exit(0);
	}
	else{
		exit(0);
	}
}

void ctrlC(){
	kill(pid, SIGINT);
}

void shellEOF(){
	exit(1);
}

void shellExitHandler(){
	exit(1);
}

int readWrite(int rfd, int wfd){
	char buf[bufSize];
	int bufCount = 0;
	int bufIndex = 0;
	while(1){
		// Read characters into the read buffer
		bufCount += read(rfd, buf+bufCount, bufSize-bufCount);
		if(bufCount == -1){
			perror("Error");
			exit(EXIT_FAILURE);
		}
		else if(bufCount == 0 && rfd == pipe2[0]){
			shellEOF();
		}
		while(bufIndex < bufCount){
			char writeBuf[10];
			writeBuf[0] = buf[bufIndex];
			int writeBufSize = 1;
			char shellBuf[10];
			shellBuf[0] = buf[bufIndex];
			int shellBufSize = 1;
			// Check for ^D
			if(buf[bufIndex] == 4){
				ctrlD();
			}
			// Convert \r or \n into \r\n
			else if(buf[bufIndex] == '\r' || buf[bufIndex] == '\n'){
				writeBuf[0] = '\r';
				writeBuf[1] = '\n';
				writeBufSize = 2;
				shellBuf[0] = '\n';
			}
			// Loop through write buffer and write to specified file descriptors
			int i;
			for(i = 0; i < writeBufSize; i++){
				status = write(STDOUT_FILENO, writeBuf+i, 1);
				if(status == -1){
                               		perror("Write Error");
                               		exit(EXIT_FAILURE);
                        	}
			}
			// Loop through shell buffer and write to specified file descriptor
			if(wfd != STDOUT_FILENO){
				for(i = 0; i < shellBufSize; i++){
					status = write(wfd, shellBuf+i, 1);
					if(status == -1){
						perror("Write Error");
						exit(EXIT_FAILURE);
					}
				}
			}
			bufIndex++;
		}
		// Go back to the beginning of the read buffer
		if(bufCount > bufSize - 1){
			bufCount = 0;
			bufIndex = 0;
		}
	}
}

void* outThread(){
	close(pipe2[1]);
	readWrite(pipe2[0], STDOUT_FILENO);
	exit(0);
}

void createShell(){
	// Create pipes
	if(pipe(pipe1) == -1){
		perror("Pipe Error");
		exit(EXIT_FAILURE);
	}
	if(pipe(pipe2) == -1){
		perror("Pipe Error");
		exit(EXIT_FAILURE);
	}
	// Create new process
	pid = fork();
	if(pid == -1){
		perror("Fork Error");
		exit(EXIT_FAILURE);
	}
	// Child process running bash shell
	else if(pid == 0){
		close(pipe1[1]);
		close(STDIN_FILENO);
		dup(pipe1[0]);
		close(pipe1[0]);
		close(pipe2[0]);
		close(STDOUT_FILENO);
		dup(pipe2[1]);
		close(STDERR_FILENO);
		dup(pipe2[1]);
		close(pipe2[1]);
		execvp("/bin/bash", NULL);
	}
	else{
		status = pthread_create(&tid, NULL, outThread, NULL);
		close(pipe1[0]);
		readWrite(STDIN_FILENO, pipe1[1]);		
	}
}

int main(int argc, char** argv){
	// Parse options
	int c;
	while(1){
		static struct option long_options[] =
		{
			{"shell", no_argument, 0, 's'}
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "s", long_options, &option_index);
		if(c == -1){
			break;
		}
		switch(c){
			case 's':
				shell = 1;
				break;
			default:
				break;
		}
	}
	// Character-at-a-time, full duplex terminal I/O
	if(!isatty(0)){
		fprintf(stderr, "Error: The input file descriptor is not a terminal\n");
		exit(EXIT_FAILURE);
	}
	// Restore normal terminal modes when exiting
	status = tcgetattr(0, &savedAttr);
	if(status == -1){
		perror("Attribute Read Error");
		exit(EXIT_FAILURE);
	}
	status = atexit(restoreAttr);
	if(status != 0){
		fprintf(stderr, "Error: Could not set exit function\n");
		exit(EXIT_FAILURE);
	}
	// Put console into character-at-a-time, no echo mode
	struct termios tAttr;
	status = tcgetattr(0, &tAttr);
	if(status == -1){
		perror("Attribute Read Error");
		exit(EXIT_FAILURE);
	}
	tAttr.c_lflag &= ~(ICANON|ECHO);
	status = tcsetattr(0, TCSAFLUSH, &tAttr);
	if(status == -1){
		perror("Attribute Set Error");
		exit(EXIT_FAILURE);
	}
	// Cause read() to return when at least 1 byte is read
	tAttr.c_cc[VMIN] = 1;
	tAttr.c_cc[VTIME] = 0;
	// Setup signal handlers
	if(shell){
		signal(SIGINT, ctrlC);
		signal(SIGPIPE, shellExitHandler);
	}
	// Change implementation based on --shell option
	if(shell){
		createShell();
	}
	else{
		readWrite(STDIN_FILENO, STDOUT_FILENO);
	}
	exit(0);
}
