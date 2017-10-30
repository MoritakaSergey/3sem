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

int maxMessageSize(int msqid) {
        int msgmax, code;
        struct msqid_ds info;
        if ((code = msgctl(msqid, MSG_INFO, &info)) < 0) {
                 printf("ERROR: can\'t execute MSG_INFO command\n");
                 exit(-1);
        }
        msgmax = ((struct msginfo*)(&info))->msgmax;
        return msgmax;
}

