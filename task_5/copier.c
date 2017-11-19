#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>

size_t getFileLength(int fd) {
        struct stat buf;
        size_t size;
        int code;
        if ((code = fstat(fd, &buf)) < 0) {
                printf("ERROR: %d in fstat() at getFileLength()\n", errno);
                exit(-1);
        }
        size = buf.st_size;
        return size;
}

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

void* my_mmap(size_t length, int fd) {
        void* ptr;
        if ((ptr = mmap(NULL, length, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0)) < 0) {
                printf("ERROR: %d in my_mmap() at copyThroughMmap()\n", errno);
                exit(-1);
        }
        close(fd);
        if (ptr == MAP_FAILED) {
                printf("ERROR: %d mapping failed in my_mmap()\n", errno);
                exit(-1);
        }
        return ptr;
}

void copyThroughMmap(int fdSource, int fdDestination) {
        int i = 0;
        size_t length = getFileLength(fdSource);
        void* ptrSource;
        void* ptrDestination;

        ftruncate(fdDestination, length);

        ptrSource = my_mmap(length, fdSource);
        ptrDestination = my_mmap(length, fdDestination);

        memcpy(ptrDestination, ptrSource, length);

        munmap(ptrSource, length);
        munmap(ptrDestination, length);

        return;
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
	copyThroughMmap(fdSource, fdDestination);
	return 0;
}
