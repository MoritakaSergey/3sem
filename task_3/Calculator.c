#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>
#include<time.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<sys/types.h>

#define NUM_THREADS 4 
#define DENSITY 50000 //число точек на квадрат 1х1 площади графика.

int density; //число точек на 1х1 площади графика для каждой нити.
double result;


//структура для передачи (часто встречающихся) пар double.

typedef struct {
        double min;
        double max;
} doubledouble;


//структура для представления: 1) функции 2) её максимума и минимума на заданном отрезке.
//если максимум или минимум сложно найти, можно возвращать какую-нибудь верхнюю или нижнюю грань.

typedef struct {
        double (*func)(double);
        double (*min)(doubledouble*);
        double (*max)(doubledouble*);
} func_t;


//структура для передачи аргументов в нити.

typedef struct {
        func_t* function;
        doubledouble* limit;
} arg_t;


//структура для работы с разделяемой памятью.

typedef struct {
        double* values;
        int shmid;
        key_t key;
} shmemory_t;


//функция : возведение в квадрат

double func_sqr (double x) 
{
	double square = x*x;
	return square;
}

double max_sqr(doubledouble* limit)
{
	double max;
	max = (func_sqr(limit->max) > func_sqr(limit->min)) ? (func_sqr(limit->max)) : (func_sqr(limit->min));
	return max;
}

double min_sqr(doubledouble* limit)
{
        double min;
        if (limit->min <= 0 && limit->max >= 0)
        {
                min = 0;
                return min;
        }
        min = (func_sqr(limit->max) < func_sqr(limit->min)) ? (func_sqr(limit->max)) : (func_sqr(limit->min));
        return min;
}


//равномерно распределенная на заданном отрезке случайная величина

double get_rand_range(doubledouble* limit, unsigned int * seed) {
	double r = ((double)rand_r(seed))/RAND_MAX;
	r = (r * ((limit->max) - (limit->min))) + (limit->min);
	return r;
}


//вычисление интеграла функции в заданных пределах методом Монте-Карло в отдельной нити

double MonteCarlo (doubledouble* limit, doubledouble* range, double (*func)(double)) {
	int i = 0, numIn = 0;
	double x, y, value;
	unsigned int seed1 = (unsigned int)pthread_self();
	unsigned int seed2 = (unsigned int)time(NULL);
	int N = (int)(density*(limit->max - limit->min)*(range->max - range->min));
	for (i = 0; i < N; i++)
	{
		x = get_rand_range(limit, &seed1);
		y = get_rand_range(range, &seed2);
		value = func(x);
		if (y >= 0 && value >= y) numIn++;
		else 
		if (y <= 0 && value <= y) numIn--;
	}

	return ((double)numIn)/DENSITY;
}

pthread_mutex_t mutex; //mutex for volatile values

void* Integrate(void* args) {
	arg_t* arg = (arg_t*)args;
        doubledouble range = {(arg->function->min)(arg->limit), (arg->function->max)(arg->limit)};
	double addition = MonteCarlo(arg->limit, &range, arg->function->func);
	pthread_mutex_lock(&mutex); //
	result += addition;
	pthread_mutex_unlock(&mutex); //
	pthread_exit(NULL);
}


//функции для работы с разделяемой памятью
 
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



int main (int argc, char* argv[]) 
{
        func_t f = {func_sqr, min_sqr, max_sqr};
	pthread_t threads[NUM_THREADS];
	int i = 0;
	int rc;
	doubledouble limit;
	arg_t arg = {&f, &limit};

	shmemory_t shmemory;
	attach(&shmemory, argv[0]);

	arg.limit->min = shmemory.values[1];
	arg.limit->max = shmemory.values[2];
	
	rc = pthread_mutex_init(&mutex, NULL); //
	if (rc != 0) {
		printf("Error on mutex initialization\n");
	}
	
	srand(time(NULL));

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
	pthread_mutex_destroy(&mutex);
	shmemory.values[0] = result;
	detach(&shmemory);
	return 0;
}

