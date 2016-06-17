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
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mcrypt.h>

const int bufSize = 1;
struct termios savedAttr;
pthread_t tid;
char* logFile;
int portNum, status, sockfd;
struct sockaddr_in serverAddress;
struct hostent *server;
int shouldEncrypt = 0, keyFd, keySize;
int shouldLog = 0, logFd;
MCRYPT efd, dfd;
char* key;

void restoreAttr(){
	int shellStatus;
	tcsetattr(0, TCSANOW, &savedAttr);
	if(shouldEncrypt){
		mcrypt_generic_deinit(dfd);
		mcrypt_module_close(dfd);
		mcrypt_generic_deinit(efd);
		mcrypt_module_close(efd);
	}
}

void ctrlD(){
	pthread_cancel(tid);
	close(sockfd);
	exit(0);
}

void readEOF(){
	write(sockfd, "0", 1);
	close(sockfd);
	exit(1);
}

int readWrite(int rfd, int wfd){	
	char buf[bufSize];
	int bufCount = 0;
	int bufIndex = 0;
	while(1){
		// Read characters into the read buffer
		int readNum = read(rfd, buf+bufCount, bufSize-bufCount);
		bufCount += readNum;
		if(readNum == -1){
			perror("Error");
			exit(EXIT_FAILURE);
		}
		else if(readNum == 0){
			readEOF();
		}
		if(shouldLog && rfd == sockfd){
			char receivedString[18] = "RECEIVED X bytes: ";
			receivedString[9] = '0' + readNum;
			status = write(logFd, receivedString, 18);
			if(status == -1){
				perror("Read Error");
				exit(EXIT_FAILURE);
			}
			status = write(logFd, buf+bufCount-readNum, readNum);
			if(status == -1){
				perror("Write Error");
				exit(EXIT_FAILURE);
			}
			char newLine = '\n';
			status = write(logFd, &newLine, 1);
			if(status == -1){
				perror("Write Error");
				exit(EXIT_FAILURE);
			}
		}
		if(shouldEncrypt && rfd == sockfd){
			status = mdecrypt_generic(dfd, buf+bufCount-readNum, readNum);
			if(status != 0){
				fprintf(stderr, "Error While Decrypting");
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
			// Check for ^D
			if(buf[bufIndex] == 4 && rfd == STDIN_FILENO){
				ctrlD();
			}
			else if(buf[bufIndex] == 4 && rfd != STDIN_FILENO){
				readEOF();
			}
			// Convert \r or \n into \r\n
			else if(buf[bufIndex] == '\r' || buf[bufIndex] == '\n'){
				writeBuf[0] = '\r';
				writeBuf[1] = '\n';
				writeBufSize = 2;
				shellBuf[0] = '\n';
			}
			// Loop through write buffer and write to specified file descriptors
			int i = 0;
			for(i = 0; i < writeBufSize; i++){
				status = write(STDOUT_FILENO, writeBuf+i, 1);
				if(status == -1){
					perror("Write Error");
					exit(EXIT_FAILURE);
                        	}
			}	
			// Loop through shell buffer and write to specified file descriptor
			if(wfd == sockfd){
				if(shouldEncrypt){
					status = mcrypt_generic(efd, shellBuf, shellBufSize);
					if(status != 0){
						fprintf(stderr, "Error While Encrypting");
						exit(EXIT_FAILURE);
					}
				}
				for(i = 0; i < shellBufSize; i++){
					status = write(wfd, shellBuf+i, 1);
					if(status == -1){
						perror("Write Error");
						exit(EXIT_FAILURE);
					}
					if(shouldLog){
						char sentString[14] = "SENT X bytes: ";
						sentString[5] = '1';
						status = write(logFd, sentString, 14);
						if(status == -1){
							perror("Write Error");
							exit(EXIT_FAILURE);
						}
						status = write(logFd, shellBuf+i, 1);
						if(status == -1){
							perror("Write Error");
							exit(EXIT_FAILURE);
						}
						char newLine = '\n';
						status = write(logFd, &newLine, 1);
						if(status == -1){
							perror("Write Error");
							exit(EXIT_FAILURE);
						}
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
        readWrite(sockfd, STDOUT_FILENO);
	exit(0);
}

void runProgram(){
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
			fprintf(stderr, "Mcrypt failed to open\n");
			exit(EXIT_FAILURE);
		}
		status = mcrypt_generic_init(efd, key, keySize, NULL);
		if(status < 0){
			fprintf(stderr, "Encryption Initialization Failed\n");
			exit(EXIT_FAILURE);
		}
		dfd = mcrypt_module_open("blowfish", NULL, "ofb", NULL);
		if(dfd == MCRYPT_FAILED){
			fprintf(stderr, "Mcrypt failed to open\n");
			exit(EXIT_FAILURE);
		}
		status = mcrypt_generic_init(dfd, key, keySize, NULL);
		if(status < 0){
			fprintf(stderr, "Decryption Initialization Failed\n");
			exit(EXIT_FAILURE);
		}
	}
	// Connect to server socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("Socket Creation Error");
		exit(EXIT_FAILURE);
	}
	server = gethostbyname("localhost");
	if(server == NULL){
		fprintf(stderr, "Error: The server could not be created");
		exit(EXIT_FAILURE);
	}
	memset((char *) &serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	memcpy((char *)server->h_addr, (char *)&serverAddress.sin_addr.s_addr, server->h_length);
	serverAddress.sin_port = htons(portNum);
    	if (connect(sockfd,(struct sockaddr *)&serverAddress,sizeof(serverAddress)) < 0){
        	perror("Server Connection Error");
		exit(EXIT_FAILURE);
	}
	// Start communicating with the server
	status = pthread_create(&tid, NULL, outThread, NULL);
	readWrite(STDIN_FILENO, sockfd);
	exit(0);
}

int main(int argc, char** argv){
	// Parse options
	int c;
	while(1){
		static struct option long_options[] =
		{
			{"port", required_argument, 0, 'p'},
			{"log", required_argument, 0, 'l'},
			{"encrypt", no_argument, 0, 'e'}
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "p:l:e", long_options, &option_index);
		if(c == -1){
			break;
		}
		switch(c){
			case 'p':
				portNum = atoi(optarg);
				break;
			case 'l':
				shouldLog = 1;
				logFd = creat(optarg, S_IWUSR | S_IRUSR);
				if(logFd == -1){
					perror("Log File Open Error");
					exit(EXIT_FAILURE);
				}
				break;
			case 'e':
				shouldEncrypt = 1;
				keyFd = open("my.key", O_RDONLY);
				if(keyFd == -1){
					perror("Key Open Error");
					exit(EXIT_FAILURE);
				} 
				break;
			default:
				exit(EXIT_FAILURE);
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
	signal(SIGPIPE, readEOF);
	// Change implementation based on --shell option
	runProgram();
	exit(0);
}
