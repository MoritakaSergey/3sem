#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>
#include<time.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<sys/types.h>

#define NUM_THREADS 4 
#define DENSITY 50000 //число точек на 1х1 площади графика

int density; //число точек на 1х1 площади графика для каждой нити
double result;

//предоставим саму функцию и её максимум и минимум на заданном отрезке
//если максимум или минимум сложно найти, можно возвращать какую-нибудь верхнюю или нижнюю грань

typedef struct {
	double (*func)(double);
	double (*min)(double, double);
	double (*max)(double, double);
} func_t;

//функция : возведение в квадрат
double func_sqr (double x) 
{
	double square = x*x;
	return square;
}

double max_sqr(double lowerLimit, double upperLimit)
{
	double max;
	max = (func_sqr(upperLimit) > func_sqr(lowerLimit)) ? (func_sqr(upperLimit)) : (func_sqr(lowerLimit));
	return max;
}

double min_sqr(double lowerLimit, double upperLimit)
{
        double min;
        if (lowerLimit <= 0 && upperLimit >= 0)
        {
                min = 0;
                return min;
        }
        min = (func_sqr(upperLimit) < func_sqr(lowerLimit)) ? (func_sqr(upperLimit)) : (func_sqr(lowerLimit));
        return min;
}

//структура для передачи аргументов в нити
typedef struct {
        func_t* function;
        double lowerLimit;
        double upperLimit;
} arg_t;

//равномерно распределенная на заданном отрезке случайная величина
double get_rand() { 
	double r = ((double)rand())/RAND_MAX;
	return r;
}
double get_rand_range(double min, double max) {
	double r = (get_rand() * (max - min)) + min;
	return r;
}

//вычисление интеграла функции в заданных пределах методом МонтеКарло в отдельной нити
double MonteCarlo (double lowerLimit, double upperLimit, func_t* func) {
	int i = 0;
	int numIn = 0;
	double max = (func->max)(lowerLimit, upperLimit);
	double min = (func->min)(lowerLimit, upperLimit);
	double x;
	double y;
	double value;
	int N = (int)(density*(upperLimit - lowerLimit)*(max - min));
	for (i = 0; i < N; i++)
	{
		x = get_rand_range(lowerLimit, upperLimit);
		y = get_rand_range(min, max);
		value = (func->func)(x);
		if (y >= 0 && value >= y) numIn++;
		else 
		if (y <= 0 && value <= y) numIn--;
	}

	return ((double)numIn)/DENSITY;
}

pthread_mutex_t lock; //mutex for volatile values

void* Integrate(void* args) {
	arg_t* arg = (arg_t*)args;
	double addition = MonteCarlo(arg->lowerLimit, arg->upperLimit, arg->function);
	pthread_mutex_lock(&lock); //
	result += addition;
	pthread_mutex_unlock(&lock); //
	pthread_exit(NULL);
}

//структура для работы с разделяемой памятью
typedef struct {
        double* values;
        int shmid;
        key_t key;
} shmemory_t;

void attach(shmemory_t* mem) {
        if ((mem->key = ftok("Receiver.c", 0)) < 0) {
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

int main (int argc, char* argv[]) 
{
        func_t f = {func_sqr, min_sqr, max_sqr};
	pthread_t threads[NUM_THREADS];
	double lowerLimit;
	double upperLimit;
	int i = 0;
	int rc;
	arg_t arg;

	shmemory_t shmemory;
	attach(&shmemory);

	arg.lowerLimit = lowerLimit = shmemory.values[1];
	arg.upperLimit = upperLimit = shmemory.values[2];
	arg.function = &f;
	
	srand(time(NULL));
	rc = pthread_mutex_init(&lock, NULL); //
	if (rc != 0) {
		printf("Error on mutex initialization\n");
	}
	
	density = DENSITY/NUM_THREADS;
	for (i = 0; i < NUM_THREADS; i++) {
		rc = pthread_create(&(threads[i]), NULL, Integrate, (void *)(&arg));
		if (rc) {
			printf("Error on pthread_create num %d with code %d\n", i, rc);
			exit(-1);
		}	
	}
	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	pthread_mutex_destroy(&lock); //
	shmemory.values[0] = result;
	detach(&shmemory);
	return 0;
}

