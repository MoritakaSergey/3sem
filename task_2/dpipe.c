#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>

typedef struct {
	int txd[2];
	int rxd[2];	
} dpipe_t;

int dpipe(dpipe_t dp)
{
        int channel1[2];
        int channel2[2];
        if (pipe(channel1) == -1) {
                exit(-1);
        }
        if (pipe(channel2) == -1) {
                close(channel1[0]);
                close(channel1[1]);
                exit(-1);
        }
        dp.txd[0] = channel2[1];
        dp.rxd[0] = channel1[0];
        dp.txd[1] = channel1[1];
        dp.rxd[1] = channel2[0];
        return 0;
} 

int closeGate(dpipe_t dp, int numOfGate)
{
        if (close(dp.txd[numOfGate]) + close(dp.rxd[numOfGate]) < 0) {
                return -1;
        }
        return 0;
}

int main() 
{

}       

