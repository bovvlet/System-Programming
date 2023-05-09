#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "libcoro.h"
#include "MyVector.h"

char **fileNames;
int64_t latency; 
int64_t numbOfCors; 
int64_t numbOfFiles;
int64_t maxTimePerCor;
int sorted_files = 0;

MyVector **myVectors;

typedef struct {
	int64_t id;
	int64_t switchNum;
	int64_t totalTime;
	struct timespec startTime;
	struct timespec lastCheckedTime;
} CoroInfo;

struct timespec getCurTime()
{
	struct timespec curTime;
	clock_gettime(CLOCK_MONOTONIC, &curTime);
	return curTime;
}

int64_t getDiffTime (struct timespec startTume, struct timespec endTime)
{
	return ((int64_t)(endTime.tv_sec - startTume.tv_sec) * 1000000) + ((int64_t)(endTime.tv_nsec - startTume.tv_nsec) / 1000);
}

void checkCorExTime(CoroInfo* coroInfo)
{
	int64_t gotTime = getDiffTime(coroInfo->startTime, getCurTime());
	if (gotTime > maxTimePerCor) {
		coroInfo->totalTime += gotTime;
		coroInfo->switchNum ++;
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &coroInfo->startTime);
	} else {
		struct timespec curTime = getCurTime(); 
		coroInfo->totalTime += getDiffTime(coroInfo->lastCheckedTime, curTime);
		coroInfo->lastCheckedTime = curTime;
	}
}

void heapSort (MyVector* myVector, CoroInfo* coroInfo) {
	
	if (myVector->sz <= 1) return;

	int n = myVector->sz;
	int* arr = myVector->arr;	

	// Build max heap by repeatedly sifting up elements
    for (int i = 1; i < n; i++) {
        checkCorExTime(coroInfo);
		int child = i;
        while (child > 0) {
			checkCorExTime(coroInfo);
            int parent = (child - 1) / 2;
            if (arr[child] > arr[parent]) {
                swap(myVector, child, parent);
                child = parent;
            }
            else {
                break;
            }
        }
    }

	checkCorExTime(coroInfo);
 
    // Extract max element and place at the end of array
    for (int i = n - 1; i > 0; i--) {
        checkCorExTime(coroInfo);
		swap(myVector, 0, i);
 
        // Sift down the new root element to maintain max heap
        int parent = 0;
        while (true) {
			checkCorExTime(coroInfo);
            int leftChild = 2 * parent + 1;
            int rightChild = 2 * parent + 2;
            int maxChild = parent;
            if (leftChild < i && arr[leftChild] > arr[maxChild]) {
                maxChild = leftChild;
            }
            if (rightChild < i && arr[rightChild] > arr[maxChild]) {
                maxChild = rightChild;
            }
            if (maxChild != parent) {
                swap(myVector, parent, maxChild);
                parent = maxChild;
            }
            else {
                break;
            }
        }
    }

	checkCorExTime(coroInfo);
	
}

static int
coroutine_func_f(void *context)
{
	CoroInfo *coroInfo = (CoroInfo *)context;
	
	//coroutine function
	while(sorted_files < numbOfFiles) {
	
		char* name_of_file = strdup(fileNames[sorted_files]);
		
		FILE *file;
		file = fopen(name_of_file, "r");

		if (file == NULL) {
			printf("> file %s didn't open correctly\n", name_of_file);
			free(file);
			break;
		}
		
		printf("> file %s openned by coroutine %lld\n", name_of_file, coroInfo->id);

		MyVector* V = new_vector();
		char* token = (char*)malloc(sizeof(char) * 256);
		while (fscanf(file, "%s", token) == 1) {
			int num = atoi(token);
			push_back(V, num);
		}
		free(token);
		fclose(file);

		myVectors[sorted_files++] = V;
		clock_gettime(CLOCK_MONOTONIC, &coroInfo->startTime);
		heapSort(V, coroInfo);

		file = fopen(name_of_file, "w");
		for (int i = 0; i < size(V); ++i) {
			fprintf(file, "%d ", get(V, i));
		}

		printf("> sorting of file %s finished by coroutine %lld\n", name_of_file, coroInfo->id);
		free(name_of_file);
		fclose(file);
	}
	
	printf("> Coroutine with num %lld finished it's work because there is no files left\n", coroInfo->id);
	return 0;
}

int
main(int argc, char **argv)
{
	
	if (argc < 4) {
		printf("Not enough args, we need you to write number of coroutines\n");
		printf("Then write Latency, and after list the names of files\n");
		return 1;
	}

	if (atoi(argv[1]) && atoi(argv[2])) {
		numbOfCors = atoi(argv[1]);
		latency = atoi(argv[2]);

		if (numbOfCors > 100) {
			printf("To many coroutines, it's number must be less than 100\n");
			return 1;
		}

		if (latency < numbOfCors) {
			printf("Time distributed between coroutines by given latency is less that one milisecond\n");
			printf("Please chose latency that is bigger than number of coroutines\n");
			return 1;
		}

		maxTimePerCor = latency / numbOfCors;
	} else {
		printf("Number of coroutines and latency must be nonzero integer values\n");
		return 1;
	}
	
	numbOfFiles = argc - 3;
	fileNames = malloc(numbOfFiles * sizeof(char *));
	for (int i = 3; i < argc; ++i) {
		if (access(argv[i], F_OK) == 0) {
			fileNames[i-3] = strdup(argv[i]);
		} else {
			printf("%s is not a file in current directory\n", argv[i]);
			return 1;
    	}
	}

	myVectors = malloc(numbOfFiles * sizeof(MyVector *));
	CoroInfo** coroInfoArr = malloc(numbOfCors * sizeof(CoroInfo *));
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
	/* Start several coroutines. */
	for (int i = 0; i < numbOfCors; ++i) {
		coroInfoArr[i] = malloc(sizeof(CoroInfo));
		coroInfoArr[i]->id = i;
		coroInfoArr[i]->switchNum = 0;
		coroInfoArr[i]->totalTime = 0;
		clock_gettime(CLOCK_MONOTONIC, &coroInfoArr[i]->startTime);
		coroInfoArr[i]->lastCheckedTime = coroInfoArr[i]->startTime;

		coro_new(coroutine_func_f, coroInfoArr[i]);
	}
	
	/* Wait for all the coroutines to end. */
	struct coro *c;
	coro_sched_wait();
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	free(c);
	/* All coroutines have finished. */

	for (int i = 0; i < numbOfCors; ++i) {
		printf("\n> CoroInfo about coroutine with ID: %lld\n", coroInfoArr[i]->id);
		printf("   -> Total time took %lld\n", coroInfoArr[i]->totalTime);
		printf("   -> Numbers of switches %lld\n", coroInfoArr[i]->switchNum);
		
		// freeing coroutine with index = i
		free(coroInfoArr[i]);
	}
	free(coroInfoArr);

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */
	FILE *file;
	file = fopen("result.txt", "w");
		
	int *indexesOfVectors = malloc(numbOfFiles * sizeof(int));
	for (int i = 0; i < numbOfFiles; ++i) {
		indexesOfVectors[i] = 0;
	}
	
	int minInd = -1;
	while (1) {
		minInd = -1;
		for (int i = 0; i < numbOfFiles; ++i) {
			if (indexesOfVectors[i] == myVectors[i]->sz)
				continue;
			
			if (minInd == -1 || myVectors[i]->arr[indexesOfVectors[i]] < myVectors[minInd]->arr[indexesOfVectors[minInd]])
				minInd = i;
		}
		
		if (minInd == -1)
			break;
		
		fprintf(file, "%d ", myVectors[minInd]->arr[indexesOfVectors[minInd]]);
		++ indexesOfVectors[minInd];
	}

	for (int i = 0; i < numbOfFiles; ++i) {
		freeMyVector(myVectors[i]);
		free(fileNames[i]);
	}

	fclose(file);
	free(myVectors);
	free(fileNames);
	free(indexesOfVectors);
	return 0;
}
