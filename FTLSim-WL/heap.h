#ifndef HEAP_H
#define HEAP_H
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
//#include "ftlsim.h"
//struct segment {
//    int magic;                  /* 5E65e65e */
//    struct segment *next, *prev;
//    int  Np;
//    int *lba;            /* lba[0..Np-1] = LBA / -1 */
//    int  in_pool;        /* false if we're still the write frontier */
//    int  n_valid;
//    struct pool *pool;
//    int type;       /* 0 = cold, 1 = hot */
//    int erase_counts;
//    int effective_ec;
//    int read_counts;
//    //heap index
//    int ecmin_index;
//    int ecmax_index;
//    int eecmin_index;
//    int eecmax_index;
//
//};
//struct segment;
/***general methods***/
int parent(int i);
int left_child(int i);
int right_child(int i);
/***general methods***/

/***EC min heap***/
typedef struct ECMinHeap ECMinHeap;
struct ECMinHeap {
    struct segment **arr;
    // Current Size of the Heap
    int size;
    // Maximum capacity of the heap
    int capacity;
};
struct segment *get_ecmin(ECMinHeap* heap);
ECMinHeap* init_ecminheap(int capacity);
ECMinHeap* insert_ecminheap(ECMinHeap* heap, struct segment* element);
ECMinHeap* ecminheapify(ECMinHeap* heap, int index);
ECMinHeap* delete_ecminimum(ECMinHeap* heap);
ECMinHeap* ecmin_delete_element(ECMinHeap* heap, int index);
ECMinHeap* ecmin_addone_check(ECMinHeap* heap, int index);
void print_ecminheap(ECMinHeap* heap);
void free_ecminheap(ECMinHeap* heap);
/***EC min heap***/

/***EC max heap***/
typedef struct ECMaxHeap ECMaxHeap;
struct ECMaxHeap {
    struct segment **arr;
    // Current Size of the Heap
    int size;
    // Maximum capacity of the heap
    int capacity;
};
struct segment *get_ecmax(ECMaxHeap* heap);
ECMaxHeap* init_ecmaxheap(int capacity);
ECMaxHeap* insert_ecmaxheap(ECMaxHeap* heap, struct segment* element);
ECMaxHeap* ecmaxheapify(ECMaxHeap* heap, int index);
ECMaxHeap* delete_ecmaximum(ECMaxHeap* heap);
ECMaxHeap* ecmax_delete_element(ECMaxHeap* heap, int index);
ECMaxHeap* ecmax_addone_check(ECMaxHeap* heap, int index);
void print_ecmaxheap(ECMaxHeap* heap);
void free_ecmaxheap(ECMaxHeap* heap);
/***EC max heap***/

/***EEC min heap***/
typedef struct EECMinHeap EECMinHeap;
struct EECMinHeap {
    struct segment **arr;
    // Current Size of the Heap
    int size;
    // Maximum capacity of the heap
    int capacity;
};
struct segment *get_eecmin(EECMinHeap* heap);
EECMinHeap* init_eecminheap(int capacity);
EECMinHeap* insert_eecminheap(EECMinHeap* heap, struct segment* element);
EECMinHeap* eecminheapify(EECMinHeap* heap, int index);
EECMinHeap* delete_eecminimum(EECMinHeap* heap);
EECMinHeap* eecmin_delete_element(EECMinHeap* heap, int index);
EECMinHeap* eecmin_addone_check(EECMinHeap* heap, int index);
void print_eecminheap(EECMinHeap* heap);
void free_eecminheap(EECMinHeap* heap);
/***EEC min heap***/

/***EEC max heap***/
typedef struct EECMaxHeap EECMaxHeap;
struct EECMaxHeap {
    struct segment **arr;
    // Current Size of the Heap
    int size;
    // Maximum capacity of the heap
    int capacity;
};
struct segment *get_eecmax(EECMaxHeap* heap);
EECMaxHeap* init_eecmaxheap(int capacity);
EECMaxHeap* insert_eecmaxheap(EECMaxHeap* heap, struct segment* element);
EECMaxHeap* eecmaxheapify(EECMaxHeap* heap, int index);
EECMaxHeap* delete_eecmaximum(EECMaxHeap* heap);
EECMaxHeap* eecmax_delete_element(EECMaxHeap* heap, int index);
EECMaxHeap* eecmax_addone_check(EECMaxHeap* heap, int index);
void print_eecmaxheap(EECMaxHeap* heap);
void free_eecmaxheap(EECMaxHeap* heap);
/***EEC max heap***/

int test(void);
#endif