/*********************************************************************************
* Daniel Choy
* 2022 Spring
* queue.h
* Header file for Queue ADT
*********************************************************************************/

#pragma once

#include <stdbool.h>

typedef struct {
    int size;
    int head;
    int tail;
    int *buffer;
    int capacity;
} BoundedQueue;

BoundedQueue new_queue(int capacity);

void enqueue(BoundedQueue *q, int x);

void dequeue(BoundedQueue *q, int *x);

bool full_queue(BoundedQueue *q);

bool empty_queue(BoundedQueue *q);

void print_queue(BoundedQueue *q);
