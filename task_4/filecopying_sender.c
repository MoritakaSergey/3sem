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

void sendFile(char* path, int msqid) {
        int code, i;
        int file;
        int maxsize = maxMessageSize(msqid);
        struct max_msgbuf {
                long mtype;
                char data[maxsize];
        } msg;
        struct end_msgbuf {
                long mtype;
                int lastByteNum[maxsize/sizeof(int)];
        } endmsg;
        char isFileNotSent = 1;

        if ((file = open(path, O_RDONLY)) < 0) {
                printf("ERROR: can\'t open a source file\n");
                exit(-1);
        }

        msg.mtype = START;
        if ((code = msgsnd(msqid, (struct msgbuf*)(&msg), sizeof(msg) - sizeof(long), 0)) < 0) {
                printf("ERROR: can\'t send a start message\n");
                exit(-1);
        }
	
        if ((code = close(file)) < 0) {
                printf("ERROR: can\'t close the source file\n");
                exit(-1);
        }
        return;
}

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
                printf("ERROR: can\'t create message queue\n");
                exit(-1);
        }
        sendFile(argv[1], msqid);
        return 0;
}

