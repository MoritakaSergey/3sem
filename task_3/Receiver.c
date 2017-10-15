#include<stdio.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<sys/types.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/wait.h>

//структура для работы с разделяемой паматью
typedef struct {
	double* values;
	int shmid;
	key_t key;
} shmemory_t;

void attach(shmemory_t* mem, const char* name) {
	if ((mem->key = ftok(name, 0)) < 0) {
                printf("Can't generate a key for values\n");
                exit(-1);
        }
        if ((mem->shmid = shmget(mem->key, 3*sizeof(double), 0666|IPC_CREAT)) < 0) {
                printf("Can't get shared memory for values\n");
                exit(-1);
        }

        if ((mem->values = (double*)shmat(mem->shmid, NULL, 0)) == (double*)(-1)) {
                printf("Error on attach memory for values in parent\n");
                exit(-1);
        }
	return;
}

void detach(shmemory_t* mem) {
        if(shmdt(mem->values) < 0) {
                printf("Can't detach shared memory\n");
        }
        return;
}


int main(int argc, char* argv[]) {
	shmemory_t shmemory;
	pid_t pid;

	attach(&shmemory, argv[0]);

        shmemory.values[0] = 0;             //output: result
        shmemory.values[1] = atof(argv[2]); //input: lowerLimit
        shmemory.values[2] = atof(argv[3]); //input: upperLimit

	pid = fork();

	if (pid < 0) {
		printf("Error on fork\n");
		exit(-1);
	}
	if (pid > 0) {
		wait(NULL);
		printf("Receiver get value : %.3f\n", shmemory.values[0]);
		detach(&shmemory);
		exit(0);
	}
	if (pid == 0) {
		if(execvp(argv[1], argv) < 0) {
			printf("Can't handle execvp\n");
		}
		exit(-1);		
	}

}
