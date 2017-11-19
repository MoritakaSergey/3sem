#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/stat.h>
#include<errno.h>
#include<fcntl.h>

int touchDestinationFile(const char* pathname) {
	int fd;
	char handlePolicy = 1;
	if ((fd = open(pathname, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0) {
		if (errno == EEXIST) {
			while(handlePolicy) {
				printf("\"%s\" already exists, do you want to rewrite the file? [y/n]\n", pathname);
				scanf("%s", &handlePolicy);
				switch(handlePolicy) {
					case 'y':
					case 'Y':
						handlePolicy = 0;
						break;
					case 'n':
					case 'N':
						return -1;
					default:
						handlePolicy = 1;
				}
			}
			if ((fd = open(pathname, O_RDWR|O_CREAT, 0666)) >= 0) {
				return fd;
			}
		}
		printf("ERROR: %d in open() at touchDestinationFile()\n", errno);
		exit(-1);
		}
	return fd;
}


int main(int argc, char* argv[]) {
	int fdSource, fdDestination;
	if (argc < 3) {
		printf("ERROR: too few arguments\n");
		exit(0);
	}
	if ((fdSource = open(argv[1], O_RDWR|0666)) < 0) {
		printf("ERROR: %d in open() at main()\n", errno);
		exit(-1);
	}
	if ((fdDestination = touchDestinationFile(argv[2])) < 0) {
		close(fdSource);
		exit(0);
	}
	return 0;
}
