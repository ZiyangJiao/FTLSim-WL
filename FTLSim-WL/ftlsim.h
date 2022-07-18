/*
 * file:        ftlsim.h
 * description: Generic FTL simulator interface for fast FTL simulator
 *
 * Peter Desnoyers, Northeastern University, 2012
 *
 * Copyright 2012 Peter Desnoyers
 * This file is part of ftlsim.
 * ftlsim is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version. 
 *
 * ftlsim is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details. 
 * You should have received a copy of the GNU General Public License
 * along with ftlsim. If not, see http://www.gnu.org/licenses/. 
 */
#include <Python.h>
#include <time.h>

struct int_array {          /* kludge for indexed arrays */
    int val;
};

struct segment {
    int magic;                  /* 5E65e65e */
    int  id;
    struct segment *next, *prev;
    int  Np;
    int *lba;            /* lba[0..Np-1] = LBA / -1 */
    int  in_pool;        /* false if we're still the write frontier */
    int  n_valid;
    struct pool *pool;
    int type;       /* 0 = cold, 1 = hot */
    int erase_counts;
    int gc_counts;
    int effective_ec;
    int read_counts;
    //heap index
    int ecmin_index;
    int ecmax_index;
    int eecmin_index;
    int eecmax_index;
    unsigned long long int last_erase_counts;

};

struct segment *segment_new(int Np);
void segment_del(struct segment *b);
void do_segment_write(struct segment *self, int page, int lba);
void do_segment_overwrite(struct segment *self, int page, int lba);

struct ftl;                    /* forward declaration */
struct ECMinHeap;
struct ECMaxHeap;
struct EECMinHeap;
struct EECMaxHeap;
typedef struct ECMinHeap ECMinHeap;
typedef struct ECMaxHeap ECMaxHeap;
typedef struct EECMinHeap EECMinHeap;
typedef struct EECMaxHeap EECMaxHeap;
typedef struct pool *(*write_selector_t)(struct ftl*, int lba);
extern write_selector_t write_select_first;
extern write_selector_t write_select_top_down;
extern write_selector_t write_select_python;
//PyObject *write_select_python_f;

typedef struct pool *(*clean_selector_t)(struct ftl*);
extern clean_selector_t clean_select_first;
extern clean_selector_t clean_select_python;
extern void return_pool(struct pool *);

struct ftl {
    int magic;                  /* Fff77711 */
    struct segment *free_list;
    struct {
        struct segment *block;
        int    page_num;
        int    host;
    } *map;
    int T, Np, U, bad_blocks;
    int int_writes, ext_writes;
    int nfree, minfree;
    int npools;
    struct pool *pools[10];
    write_selector_t get_input_pool;
    PyObject *get_input_pool_arg;
    clean_selector_t get_pool_to_clean;
    PyObject *get_pool_to_clean_arg;
    int write_seq;
    //for Wl
    unsigned long long int erase_counts;
    int TBW;
    int wl_counts;
    int wl_threshold;
    int wl_threshold_ap; // for OBP WL
    int wl_activated;
    int wl_writes;
    int wl_ds;
//    struct segment *hottest;
//    struct segment *second_hottest;
//    struct segment *coldest;
//    struct segment *second_coldest;
    //for RR
    int rr_counts;
    int rr_threshold;
    int rr_writes;
    int ext_reads;
    //dual pool heap
    ECMinHeap* hot_ec_min;
    ECMaxHeap* hot_ec_max;
    EECMinHeap* hot_eec_min;
    ECMinHeap* cold_ec_min;
    EECMaxHeap* cold_eec_max;
    int hpr;
    int cpr;
    //old block protection heap
    ECMinHeap* ec_min;
    // pwl
    int endurance;
    // separate GCed data and host-write data
    int gc;
    int gc_index;
    struct segment *gc_frontier;
    // capacity var
    int shrinkable;
    //CV overhead
    float time;
    // DAGC
    int max_erase;


};

struct getaddr;                 /* forward declaration */
struct ftl *ftl_new(int T, int Np);
void ftl_del(struct ftl*);
void do_put_blk(struct ftl *self, struct segment *blk);
struct segment *do_get_blk(struct ftl *self);
void do_ftl_run(struct ftl *ftl, struct getaddr *addrs, int count);
void do_ftl_read(struct ftl *ftl, int addrs);
int do_ftl_write(struct ftl *ftl, int addrs);
void do_ftl_build_heap(struct ftl *ftl);

struct getaddr {
    int (*getaddr)(void *self);
    void (*del)(void *self);
    void *private_data;
};

struct pool {
    int magic;                  /* 60016001 */
    struct ftl *ftl;
    struct segment *frontier, *tail;
    int Np, int_writes, ext_writes, i;
    int pages_valid, pages_invalid, length;
    void (*addseg)(struct pool *self, struct segment *blk);
    void (*insertseg)(struct pool *self, struct segment *blk);
    struct segment * (*getseg)(struct pool *self);
    void (*write)(struct ftl*, struct pool*, int);
    void (*run)(struct pool*, struct getaddr*, int);
    void (*del)(struct pool*);
    double (*tail_utilization)(struct pool*);
    struct pool *next_pool;
    int last_write;
    double rate;
    struct segment *bins; /* for greedy - [i] has 'i' valid pages */
    int min_valid;
    struct segment *(*next_segment)(struct pool *pool, struct segment *s);
    struct segment *(*tail_segment)(struct pool *pool);
};

extern double ewma_rate;
struct pool *lru_pool_new(struct ftl *, int Np);
struct pool *greedy_pool_new(struct ftl *, int Np);

extern int err_occurred;

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
