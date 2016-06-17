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
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mcrypt.h>

const int bufSize = 1;
struct termios savedAttr;
pid_t pid;
pthread_t tid;
int portNum, status;
int pipe1[2], pipe2[2];
int sockfd, newsockfd, clientLength;
struct sockaddr_in serverAddress, clientAddress;
int shouldEncrypt = 0, keyFd, keySize;
MCRYPT efd, dfd;
char* key;

void closeAllFd(){
	close(sockfd);
	close(0);
	close(1);
	close(2);
	close(pipe1[1]);
	close(pipe2[0]);
}

void shellEOF(){
	closeAllFd();
	kill(pid, SIGKILL);
	exit(2);
}

void shellExitHandler(){
	closeAllFd();
	kill(pid, SIGKILL);
	exit(2);
}

void serverEOF(){
	closeAllFd();
	kill(pid, SIGKILL);
	exit(1);
}

int readWrite(int rfd, int wfd){
	char buf[bufSize];
	int bufCount = 0;
	int bufIndex = 0;
	while(1){
		// Read characters into the read buffer
		ssize_t readNum = read(rfd, buf+bufCount, bufSize-bufCount);
		bufCount += readNum;
		if(readNum == -1 && rfd == 0){
			serverEOF();
		}
		else if(readNum == -1 && rfd != 0){
			perror("Error reading from the pipe");
			exit(EXIT_FAILURE);
		}
		else if(readNum == 0 && rfd == pipe2[0]){
			shellEOF();
		}
		else if(readNum == 0 && rfd == 0){
			serverEOF();
		}
		if(shouldEncrypt && rfd == 0){
                        status = mdecrypt_generic(dfd, buf+bufCount-readNum, readNum);
                        if(status != 0){
                                perror("Error While Decrypting");
                                exit(EXIT_FAILURE);
                        }
		}
		while(bufIndex < bufCount){
			char writeBuf[9];
			writeBuf[0] = buf[bufIndex];
			int writeBufSize = 1;
			char shellBuf[9];
			shellBuf[0] = buf[bufIndex];
			int shellBufSize = 1;
			// Loop through write buffer and write to specified file descriptors
			int i;
			if(wfd == 1){
				if(shouldEncrypt){
                                        status = mcrypt_generic(efd, writeBuf, writeBufSize);
                                        if(status != 0){
                                                perror("Error While Encrypting");
                                                exit(EXIT_FAILURE);
                                        }
                                }
				for(i = 0; i < writeBufSize; i++){
					status = write(wfd, writeBuf+i, 1);
					if(status == -1){
                               			perror("Write Error");
                               			exit(EXIT_FAILURE);
                        		}
				}
			}
			// Loop through shell buffer and write to specified file descriptor
			else{
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
	readWrite(pipe2[0], 1);
	exit(0);
}

void createShell(){
	// Set up encryption
        if(shouldEncrypt){
                struct stat keyBuf;
                status = fstat(keyFd, &keyBuf);
                if(status == -1){
                        perror("Error Determining Key Size");
                        exit(EXIT_FAILURE);
                }
                key = (char*)malloc(keyBuf.st_size * sizeof(char*));
                status = read(keyFd, key, keyBuf.st_size);
                keySize = keyBuf.st_size;
                if(status == -1){
                        perror("Error Reading Key");
                        exit(EXIT_FAILURE);
                }
                efd = mcrypt_module_open("blowfish", NULL, "ofb", NULL);
                if(efd == MCRYPT_FAILED){
                        perror("Mcrypt failed to open");
                        exit(EXIT_FAILURE);
                }
                status = mcrypt_generic_init(efd, key, keySize, NULL);
                if(status < 0){
                        perror("Encryption Initialization Failed");
                        exit(EXIT_FAILURE);
                }
                dfd = mcrypt_module_open("blowfish", NULL, "ofb", NULL);
                if(dfd == MCRYPT_FAILED){
                        perror("Mcrypt failed to open");
                        exit(EXIT_FAILURE);
                }
                status = mcrypt_generic_init(dfd, key, keySize, NULL);
                if(status < 0){
                        perror("Decryption Initialization Failed");
                        exit(EXIT_FAILURE);
                }
        }
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
		dup2(pipe1[0], STDIN_FILENO);
		close(pipe1[0]);
		close(pipe2[0]);
		dup2(pipe2[1], STDOUT_FILENO);
		dup2(pipe2[1], STDERR_FILENO);
		close(pipe2[1]);
		execvp("/bin/bash", NULL);
	}
	else{
		status = pthread_create(&tid, NULL, outThread, NULL);
		close(pipe1[0]);
		readWrite(0, pipe1[1]);		
	}
}

int main(int argc, char** argv){
	// Parse options
	int c;
	while(1){
		static struct option long_options[] =
		{
			{"port", required_argument, 0, 'p'},
			{"encrypt", no_argument, 0, 'e'}
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "p:e", long_options, &option_index);
		if(c == -1){
			break;
		}
		switch(c){
			case 'p':
				portNum = atoi(optarg);
				break;
			case 'e':
				shouldEncrypt = 1;
                                keyFd = open("my.key", O_RDONLY);
				break;
			default:
				exit(EXIT_FAILURE);
		}
	}
	// Create the server socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	memset((char *) &serverAddress, '0', sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(portNum);
	bind(sockfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	listen(sockfd, 5);
	clientLength = sizeof(clientAddress);
	newsockfd = accept(sockfd, (struct sockaddr *) &clientAddress, &clientLength);
	// Redirect stdin, stdout, and stderr to the socket
	dup2(newsockfd, 0);
	dup2(newsockfd, 1);
	dup2(newsockfd, 2);
	close(newsockfd);
	// Setup signal handlers
	signal(SIGPIPE, shellExitHandler);
	// Create the shell process
	createShell();
	exit(0);
}
