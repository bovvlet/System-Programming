#ifndef MYVECTOR_H
#define MYVECTOR_H

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	int sz;
	int max_sz;
	int* arr;
} MyVector;

MyVector* new_vector() {
	MyVector* obj = malloc(sizeof(MyVector));
	obj->sz = 0;
	obj->max_sz = 4;
	obj->arr = malloc(sizeof(int) * 4);
}

void push_back(MyVector* myVector, int x) {
	while (myVector->sz >= myVector->max_sz) {
		myVector->max_sz *= 2;
		myVector->arr = realloc(myVector->arr, sizeof(int) * myVector->max_sz);

		if (myVector->arr == NULL) {
			printf("Error: Alocation of memory didn't work\n");
            exit(EXIT_FAILURE);
		}
	}
	myVector->arr[myVector->sz ++] = x;
}

bool has_elem(MyVector* myVector, int pos) {
	return myVector->sz > pos;
}

int get(MyVector* myVector, int pos) {
	if (has_elem(myVector, pos))
		return myVector->arr[pos];
	
	printf("Trying to access index out of bound\n");
	exit(EXIT_FAILURE);
}

void swap(MyVector* myVector, int l, int r) {
	if (!has_elem(myVector, l) || !has_elem(myVector, r)) {
		printf("Error: Index of Vector out of bound\n");
        exit(EXIT_FAILURE);
	}
	
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

void freeMyVector(MyVector* myVector) {
	free(myVector->arr);
	free(myVector);
}

#endif /*MYVECTOR_H*/