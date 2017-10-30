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

void receiveFile(char* path, int msqid) {
	int code, i;
        int file;
        int maxsize = maxMessageSize(msqid);
        struct max_msgbuf {
                long mtype;
                char data[maxsize];
        } msg;
        struct msqid_ds buf;
        char isFileNotReceived = 1;

        if ((file = creat(path, 0666)) < 0) {
                printf("ERROR: can\'t create a destination file\n");
                exit(-1);
        }

	if ((code = close(file)) < 0) {
                printf("ERROR: can\'t close the destination file\n");
            	exit(-1);
        }
        if ((code = msgctl(msqid, IPC_RMID, &buf)) < 0) {
                printf("ERROR: can\'t remove the message queue\n");
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
                printf("ERROR: can\'t join the message queue\n");
                exit(-1);
        }
        receiveFile(argv[1], msqid);
        return 0;
}

