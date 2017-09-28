#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>

typedef struct {
	int txd[2];
	int rxd[2];	
} dpipe_t;
