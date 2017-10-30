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

#define ANYTYPE 0
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

void receiveFile(char* path, int msqid);

int main(int argc, char* argv[]) {
        if (argc < 2) {
                printf("ERROR: too few arguments");
                exit(-1);
        }
        int msqid;
        key_t key;
        if ((key = ftok("./", 0)) < 0) {
                printf("ERROR: can\'t generate a key\n");
                exit(-1);
        }
        if ((msqid = msgget(key, 0666|IPC_CREAT)) < 0) {
                printf("ERROR: can\'t join the message queue\n");
                exit(-1);
        }
        receiveFile(argv[1], msqid);
        return 0;
}

