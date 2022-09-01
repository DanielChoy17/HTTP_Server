/*********************************************************************************
* Daniel Choy
* 2022 Spring
* queue.c
* Implementation file for Queue ADT
*********************************************************************************/

#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

BoundedQueue new_queue(int capacity) {
    BoundedQueue q;
    q.size = 0;
    q.head = 0;
    q.tail = 0;
    q.buffer = malloc(capacity * sizeof(int));
    q.capacity = capacity;
    return q;
}

void enqueue(BoundedQueue *q, int x) {
    q->buffer[q->tail] = x;
    q->tail = (q->tail + 1) % q->capacity;
    q->size = q->size + 1;
}

void dequeue(BoundedQueue *q, int *x) {
    *x = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size = q->size - 1;
}

bool full_queue(BoundedQueue *q) {
    return q->size == q->capacity;
}

bool empty_queue(BoundedQueue *q) {
    return q->size == 0;
}

void print_queue(BoundedQueue *q) {
    printf("[");
    for (int i = 0; i < q->size; i++) {
        printf("%d", q->buffer[(q->head + i) % q->capacity]);
        if (i + 1 != q->size) {
            printf(", ");
        }
    }
    printf("]\n");
}
