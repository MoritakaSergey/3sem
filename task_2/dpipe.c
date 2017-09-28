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
	return 0;
}
