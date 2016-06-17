#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>

void faultHandler(int n){
	if(n == SIGSEGV){
		perror("Segmentation Fault Caught");
		exit(3);
	}
}

int main(int argc, char** argv){
	int shouldSegFault = 0;
	int ifd, ofd, c;
	while(1){
		static struct option longOptions[] = {
			{"catch",	no_argument,		0, 	'c'},
			{"segfault",	no_argument, 		0,	's'},
			{"input",	required_argument, 	0,	'i'},
			{"output",	required_argument,	0,	'o'}
		};
		int optionIndex = 0;
		c = getopt_long (argc, argv, "csi:o:", longOptions, &optionIndex);
		if(c == -1){
			break;
		}
		switch(c){
			case 'c':
				signal(SIGSEGV, faultHandler);
				break;
			case 's':
				shouldSegFault = 1;
				break;
			case 'i':
				ifd = open(optarg, O_RDONLY);
				if(ifd < 0){
					perror("Error");
					exit(1);
				}
				else if (ifd >= 0){
					close(0);
					dup(ifd);
					close(ifd);	
				}
				break;
			case 'o':
				ofd = creat(optarg, S_IRWXU);
				if(ofd < 0){
					perror("Error");
					exit(2);
				}
				else if(ofd >= 0){
					close(1);
					dup(ofd);
					close(ofd);
				}
				break;
			default:
				abort();
		}		
	}
	if(shouldSegFault == 1){
		char* nullVar = NULL;
		*nullVar = 'a';
	}
	int r = 1;
	char buf[1];
	r = read(0, buf, 1);
	while(r == 1){
		write(1, buf, 1);
		r = read(0, buf, 1);	
	}	
	exit(0);
}
