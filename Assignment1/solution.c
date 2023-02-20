#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcoro.h"

/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */

int sorted_files = 3;
int n_files;
char** files;

int coroutine_numb;
int target_latency;
int* coroutine_cur_time;
int* coroutine_total_time;
int cur_coroutine = 0;

typedef struct MyVector MyVector; 
struct MyVector
{
	int sz;
	int max_sz;
	int* arr;
};

MyVector* new_vector() {
	MyVector* obj = malloc(sizeof(MyVector));
	obj->sz = 0;
	obj->max_sz = 4;
	obj->arr = malloc(sizeof(int) * 4);
}

void push_back(MyVector* myVector, int x) {
	myVector->arr[myVector->sz ++] = x;
	if (myVector->sz == myVector->max_sz) {
		myVector->max_sz *= 2;
		myVector->arr = realloc(myVector->arr, sizeof(int) * myVector->max_sz);
	}
}

bool has_elem(MyVector* myVector, int pos) {
	return myVector->sz > pos;
}

int get(MyVector* myVector, int pos) {
	if (has_elem(myVector, pos))
		return myVector->arr[pos];
	return NULL;
}

void swap(MyVector* myVector, int l, int r) {
	if (!has_elem(myVector, l) || !has_elem(myVector, r))
		return;
	
	int box = myVector->arr[l];
	myVector->arr[l] = myVector->arr[r];
	myVector->arr[r] = box;
}

int size(MyVector* myVector) {
	return myVector->sz;
}

bool isEmpty(MyVector* myVector) {
	return myVector->sz == 0;
}

void delete(MyVector* myVector) {
	free(myVector->arr);
	free(myVector);
}

void check_before_yield(int i) {
	if (coroutine_cur_time[i] >= target_latency) {
		coroutine_cur_time[i] = 0;
		coro_yield();
	}
	return;
}

void heapSort(MyVector* myVector, int cur_coroutine)
{   
	if (myVector->sz <= 1) return;

	int n = myVector->sz;
	int* arr = myVector->arr;	

	for (int i = 1; i < n; i++)
    {
        if (arr[i] > arr[(i - 1) / 2])
        {
            int j = i;
     
            while (arr[j] > arr[(j - 1) / 2])
            {
                swap(myVector, j, (j - 1) / 2);
                j = (j - 1) / 2;
            }
        }
    }
 
    for (int i = n - 1; i > 0; i--)
    {
        swap(myVector, 0, i);
     
        int j = 0, index;
         
        do
        {
			index = (2 * j + 1);
             
            if (arr[index] < arr[index + 1] && index < (i - 1))
                index++;
         
            if (arr[j] < arr[index] && index < i)
				swap(myVector, j, index);
                
            j = index;
         
        } while (index < i);
    }
}

static int
coroutine_func_f(void *context)
{
	struct coro *this = coro_this();
	char *name = context;
	
	//getting current coroutine numb
	// char* numb[10];
	// strncpy(numb,context+5, 5);
	// cur_coroutine = atoi(numb);
	
	//coroutine function
	while(sorted_files < n_files) {
		char* name_of_file = strdup(files[sorted_files]);
		
		FILE *file;
		file = fopen(name_of_file, "r");

		if (file == NULL) {
			printf("file %s didn't open correctly\n", name_of_file);
			break;
		}
		
		printf("file %s openned\n", name_of_file);

		MyVector* V = new_vector();
		char* token;
		while (fscanf(file, "%s", token) == 1) {
			int num = atoi(token);
			push_back(V, num);
		}	
		fclose(file);
		heapSort(V, cur_coroutine);

		file = fopen(name_of_file, "w");
		for (int i = 0; i < size(V); ++i) {
			fprintf(file, "%d ", get(V, i));
		}
		printf("\n");

		fclose(file);
		delete(V);
		sorted_files++;
	}


	printf("%s finished it's work\n", context);

	free(name);
	return 0;
}

int
main(int argc, char **argv)
{
	files = argv;
	n_files = argc;

	target_latency = atoi(argv[1]);
	coroutine_numb = atoi(argv[2]);
	coroutine_cur_time = realloc(coroutine_cur_time, sizeof(int) * coroutine_numb);
	coroutine_total_time = realloc(coroutine_total_time, sizeof(int) * coroutine_numb);

	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
	/* Start several coroutines. */
	for (int i = 0; i < coroutine_numb; ++i) {
		/*
		 * The coroutines can take any 'void *' interpretation of which
		 * depends on what you want. Here as an example I give them
		 * some names.
		 */
		char name[16];
		sprintf(name, "coro_%d", i);
		/*
		 * I have to copy the name. Otherwise all the coroutines would
		 * have the same name when they finally start.
		 */
		coro_new(coroutine_func_f, strdup(name));
	}
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	/* All coroutines have finished. */

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */

	return 0;
}
