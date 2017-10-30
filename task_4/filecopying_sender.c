#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<fcntl.h>
#include<errno.h>
#include<string.h>
#include<unistd.h>

#define _GNU_SOURCE

#define START 3
#define DATA 2
#define END 1

