#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>

typedef struct {
	int txd[2];
	int rxd[2];	
} dpipe_t;

int dpipe(dpipe_t* dp)
{
        int channel1[2];
        int channel2[2];
        if (pipe(channel1) == -1) {
                return -1;
        }
        if (pipe(channel2) == -1) {
                return -1;
        }
        dp->txd[0] = channel1[1];
        dp->rxd[0] = channel2[0];
        dp->txd[1] = channel2[1];
        dp->rxd[1] = channel1[0];
        return 0;
} 

int closeGate(dpipe_t dp, int numOfGate)
{
        if (close(dp.txd[numOfGate]) + close(dp.rxd[numOfGate]) < 0) {
                return -1;
        }
        return 0;
}

void send(int fd, char* string)
{
        int i = 0;
        while(write(fd, string + i++, (string[i] == EOF) ? 0 : 1) > 0);
        printf("pid==%d sent: %s\n", getpid(), string);
        return;
}

void receive(int fd, char* buffer)
{
        int i = 0;
        while(read(fd, buffer + i++, 1) > 0);
        printf("pid==%d received: %s\n", getpid(), buffer);
        return;
}

int main() 
{
	dpipe_t dp;
        pid_t pid;
        int size;
        char buffer[100];
        char *string;
        if (dpipe(&dp) < 0) {
                printf("Can\'t create dpipe\n");
                exit(-1);
        }

        pid = fork();

        if (pid < 0) {
                printf("Can\'t fork child\n");
                exit(-1);
        } else if (pid > 0) {
                string = "Can you answer me?";
                closeGate(dp, 0);

                send(dp.txd[1], string);
                close(dp.txd[1]);       
                receive(dp.rxd[1], buffer);
                close(dp.rxd[1]);

                printf("Parent exit\n");
        } else {
                string = "Yes, I can, because this pipe is duplex.";
                closeGate(dp, 1);

                receive(dp.rxd[0], buffer);
                close(dp.rxd[0]);
                send(dp.txd[0], string);
                close(dp.txd[0]);

                printf("Child exit\n");
        }
	return 0;
}
