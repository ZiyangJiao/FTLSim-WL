/*
 * file:        ftlsim.c
 * description: Simple, fast multi-pool FTL simulator
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

#include <stdio.h>
#include <stdlib.h>
//#include <assert.h>
#include <Python.h>

#include "ftlsim.h"

#define assert(x) {if (!(x)) *(char*)0 = 0;}

#include <setjmp.h>
jmp_buf bailout_buf;
int err_occurred;

static void list_add(struct segment *b, struct segment *list)
{
    b->next = list;
    b->prev = list->prev;
    list->prev->next = b;
    list->prev = b;
}

static void list_rm(struct segment *b)
{
    b->prev->next = b->next;
    b->next->prev = b->prev;
    b->next = b->prev = b;
}

int list_len(struct segment *list)
{
    struct segment *p;
    int i = 0;
    for (p = list->next; p != list; p = p->next)
        i++;
    return i;
}

static int list_empty(struct segment *b)
{
    return b->next == b;
}

static struct segment *list_pop(struct segment *list)
{
    struct segment *b = list->next;
    list_rm(b);
    return b;
}

struct segment *segment_new(int Np)
{
    int i;
    struct segment *fb = calloc(sizeof(*fb), 1);
    fb->magic = 0x5E65e65e;
    fb->Np = Np;
    fb->lba = calloc(Np * sizeof(int), 1);
    for (i = 0; i < Np; i++)
        fb->lba[i] = -1;

    return fb;
}

void segment_del(struct segment *fb)
{
    fb->magic = 0;
    free(fb);
}

void do_segment_write(struct segment *self, int page, int lba)
{
    assert(page < self->Np && page >= 0 && self->lba[page] == -1);
    self->lba[page] = lba;
    self->pool->ftl->map[lba].block = self;
    self->pool->ftl->map[lba].page_num = page;
    self->n_valid++;
}

void do_segment_overwrite(struct segment *self, int page, int lba)
{
    assert(page < self->Np && page >= 0 && self->lba[page] == lba);
    self->lba[page] = -1;
    self->n_valid--;
    if (self->pool && self->in_pool) {
        self->pool->pages_valid--;
        self->pool->pages_invalid++;

        if (self->pool->bins) {
            list_rm(self);
            list_add(self, &self->pool->bins[self->n_valid]);
            if (self->n_valid < self->pool->min_valid)
                self->pool->min_valid = self->n_valid;
        }
    }
}

struct ftl *ftl_new(int T, int Np)
{
    struct ftl *ftl = calloc(sizeof(*ftl), 1);
    ftl->magic = 0xFff77711;
    ftl->T = T;
    ftl->Np = Np;
    ftl->map = calloc(sizeof(*ftl->map)*T*Np, 1);

    return ftl;
}

void ftl_del(struct ftl *ftl)
{
    struct segment *b;
    ftl->magic = 0;
    while ((b = do_get_blk(ftl)) != NULL)
        segment_del(b);
    free(ftl->map);
    free(ftl);
}

void do_put_blk(struct ftl *self, struct segment *blk)
{
    blk->next = self->free_list;
    self->free_list = blk;
    self->nfree++;
}

struct segment *do_get_blk(struct ftl *self)
{
    struct segment *val = self->free_list;
    if (val != NULL) {
        self->free_list = val->next;
        self->nfree--;
    }
    return val;
}

static void check_new_segment(struct ftl *ftl, struct pool *pool)
{
    assert(pool->i <= pool->Np);
    if (pool->i >= pool->Np) {
        struct segment *b = do_get_blk(ftl);
        pool->addseg(pool, b);
    }
}

void do_ftl_run(struct ftl *ftl, struct getaddr *addrs, int count)
{
    int i, j;
    if (setjmp(bailout_buf) != 0) {
        err_occurred = 1;
        return;
    }
    for (i = 0; i < count; i++) {
        int lba = addrs->getaddr(addrs);
        if (lba == -1)
            return;
        assert(lba >= 0 && lba < ftl->T * ftl->Np);
        struct pool *pool = NULL;
        if (ftl->get_input_pool)
            pool = ftl->get_input_pool(ftl, lba);
        if (pool == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "ftl: input_pool error");
            longjmp(bailout_buf, 1);
        }
        ftl->ext_writes++;
        ftl->write_seq++;

        pool->write(ftl, pool, lba);
        check_new_segment(ftl, pool);

        while (ftl->nfree < ftl->minfree) {
            pool = ftl->get_pool_to_clean(ftl);
            struct segment *b = pool->getseg(pool);
            if (b == NULL) {
                PyErr_SetString(PyExc_RuntimeError, "ftl: segment cleaning error");
                longjmp(bailout_buf, 1);
            }
            struct pool *next = pool->next_pool;
            if (pool == NULL) {
                PyErr_SetString(PyExc_RuntimeError, "ftl: pool.next_pool = None");
                longjmp(bailout_buf, 1);
            }

            for (j = 0; j < b->Np; j++)
                if (b->lba[j] != -1) {
                    next->write(ftl, next, b->lba[j]);
                    check_new_segment(ftl, next);
                }
            do_put_blk(ftl, b);
        }
    }
}

/* cleaning - grab the tail of the pool. the caller of this function
 * is responsible for copying the remaining valid pages.
 */
struct segment *lru_pool_getseg(struct pool *pool)
{
    assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
    if (pool->tail == NULL || pool->tail == pool->frontier) {
        PyErr_SetString(PyExc_RuntimeError, "ftl: lru.get_seg: empty");
        longjmp(bailout_buf, 1);
    }
    struct segment *val = pool->tail;
    assert(val->in_pool);
    assert(val->pool == pool);
    
    pool->tail = val->prev;
    if (pool->tail != NULL)
        pool->tail->next = NULL;
    val->in_pool = 0;

    pool->pages_valid -= val->n_valid;
    pool->pages_invalid -= (pool->Np - val->n_valid);
    assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);

    val->pool = NULL;
    pool->length--;
    assert(pool->length >= 0);
    
    return val;
}

double lru_tail_utilization(struct pool *pool)
{
    if (pool->tail == NULL || pool->tail == pool->frontier)
        return 1.0;
    return (double)pool->tail->n_valid / (double)pool->tail->Np;
}

struct segment *lru_tail_seg(struct pool *pool)
{
    if (pool->tail == NULL || pool->tail == pool->frontier)
        return NULL;
    return pool->tail;
}
    
/* After the current write frontier fills, call this function to move
 * it to the pool and provide a new write frontier.
 */
void lru_pool_addseg(struct pool *pool, struct segment *fb)
{
    assert(pool->frontier == NULL || pool->i == pool->Np);
    assert(fb->in_pool == 0);

    pool->length++;
    fb->pool = pool;

    pool->i = 0;                /* page pointer for new block */

    fb->prev = NULL;            /* link onto head of dbl linked list */
    fb->next = pool->frontier;
    if (pool->frontier != NULL) {
        pool->frontier->in_pool = 1; /* old frontier is now in pool */
        pool->frontier->prev = fb;
        pool->pages_valid += pool->frontier->n_valid;
        pool->pages_invalid += (pool->Np - pool->frontier->n_valid);
        assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
    }
    pool->frontier = fb;

    if (pool->tail == NULL)     /* handle initial case */
        pool->tail = fb;
    
}

/* insert a block into the pool *after* the frontier.
 */
void lru_pool_insertseg(struct pool *pool, struct segment *blk)
{
    pool->length++;
    blk->pool = pool;
    blk->prev = pool->frontier;
    blk->next = pool->frontier->next;
    pool->frontier->next = blk;
    pool->pages_valid += blk->n_valid;
    pool->pages_invalid += (pool->Np - blk->n_valid);
}

void lru_pool_del(struct pool *pool)
{
    pool->magic = 0;
    free(pool);
}
double ewma_rate = 0.95;

/* this is identical to greedy_int_write - need to merge them
 */
static void lru_int_write(struct ftl *ftl, struct pool *pool, int lba)
{
    assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
    ftl->int_writes++;
    struct segment *b = ftl->map[lba].block;
    int page = ftl->map[lba].page_num;
    if (b != NULL) {
        if (b->pool != NULL && b->pool != pool) {
            for (; b->pool->last_write < b->pool->ftl->write_seq; b->pool->last_write++)
                b->pool->rate *= ewma_rate;
            b->pool->rate += (1-ewma_rate);
        }
        do_segment_overwrite(b, page, lba);
    }
    do_segment_write(pool->frontier, pool->i++, lba);
    assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
}

struct segment *lru_next_seg(struct pool *pool, struct segment *prev)
{
    if (prev == NULL)
        prev = pool->frontier;
    return prev->next;
}

struct pool *lru_pool_new(struct ftl *ftl, int Np)
{
    struct pool *val = calloc(sizeof(*val), 1);
    val->magic = 0x60016001;
    
    val->ftl = ftl;
    val->Np = Np;

    int i = ftl->npools++;
    ftl->pools[i] = val;

    val->addseg = lru_pool_addseg;
    val->insertseg = lru_pool_insertseg;
    val->getseg = lru_pool_getseg;
    val->write = lru_int_write;
    val->del = lru_pool_del;
    val->tail_utilization = lru_tail_utilization;
    val->next_segment = lru_next_seg;
    val->tail_segment = lru_tail_seg;
    
    return val;
}

static int greedy_tail_n_valid(struct pool *pool)
{
    int i;
    for (i = pool->min_valid; i < pool->Np; i++)
        if (!list_empty(&pool->bins[i]))
            break;
    pool->min_valid = i;
    return i;
}

static double greedy_tail_utilization(struct pool *pool)
{
    return (double)greedy_tail_n_valid(pool) / (double)pool->Np;
}

static struct segment *greedy_tail_segment(struct pool *pool)
{
    int i = greedy_tail_n_valid(pool);
    if (i > pool->Np)
        return NULL;
    else
        return pool->bins[i].next;
}

static struct segment *greedy_pool_getseg(struct pool *pool)
{
    int i = greedy_tail_n_valid(pool);
    if (i == pool->Np) {
        PyErr_SetString(PyExc_RuntimeError, "ftl: greedy: pool full");
        longjmp(bailout_buf, 1);
    }
    struct segment *b = list_pop(&pool->bins[i]);
    b->in_pool = 0;

    pool->pages_valid -= b->n_valid;
    pool->pages_invalid -= (pool->Np - b->n_valid);
    assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
    b->pool = NULL;

    pool->length--;
    assert(pool->length >= 0);

    return b;
}

static void greedy_pool_addseg(struct pool *pool, struct segment *fb)
{
    assert(pool->frontier == NULL || pool->i == pool->Np);
    assert(fb->in_pool == 0);

    pool->length++;

    pool->i = 0;                /* page pointer for new block */

    struct segment *blk = pool->frontier;
    if (blk != NULL) {
        pool->frontier->in_pool = 1; /* old frontier is now in pool */
        pool->pages_valid += pool->frontier->n_valid;
        pool->pages_invalid += (pool->Np - pool->frontier->n_valid);
        assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);

        list_add(blk, &pool->bins[blk->n_valid]);
        if (blk->n_valid < pool->min_valid)
            pool->min_valid = blk->n_valid;
    }

    pool->frontier = fb;
    fb->pool = pool;
}

/* add a segment to the pool, bypassing the write frontier.
 */
static void greedy_pool_insertseg(struct pool *pool, struct segment *blk)
{
    pool->length++;

    blk->in_pool = 1;
    blk->pool = pool;
    
    pool->pages_valid += blk->n_valid;
    pool->pages_invalid += (pool->Np - blk->n_valid);

    list_add(blk, &pool->bins[blk->n_valid]);
    if (blk->n_valid < pool->min_valid)
        pool->min_valid = blk->n_valid;
}

static void greedy_int_write(struct ftl *ftl, struct pool *pool, int lba)
{
    ftl->int_writes++;
    struct segment *b = ftl->map[lba].block;
    int page = ftl->map[lba].page_num;
    if (b != NULL) {
        if (b->pool != NULL && b->pool != pool) {
            for (; b->pool->last_write < b->pool->ftl->write_seq; b->pool->last_write++)
                b->pool->rate *= ewma_rate;
            b->pool->rate += (1-ewma_rate);
        }
        do_segment_overwrite(b, page, lba);
    }
    
    do_segment_write(pool->frontier, pool->i++, lba);
}

static void greedy_pool_del(struct pool *pool)
{
    pool->magic = 0;
    free(pool->bins);
    free(pool);
}

struct segment *greedy_next_seg(struct pool *pool, struct segment *prev)
{
    if (prev == NULL)
        prev = &pool->bins[0];
    if (prev->next == &pool->bins[pool->Np])
        return NULL;
    if (prev->lba == NULL) {    /* it's a bin */
        if (list_empty(prev))
            return greedy_next_seg(pool, prev+1);
        else
            return prev->next;
    }
    if (prev->next->lba == NULL)
        return greedy_next_seg(pool, prev->next+1);
    return prev->next;
}

struct pool *greedy_pool_new(struct ftl *ftl, int Np)
{
    struct pool *pool = calloc(sizeof(*pool), 1);
    pool->magic = 0x60016002;
    pool->ftl = ftl;
    pool->Np = Np;

    int i = ftl->npools++;
    ftl->pools[i] = pool;

    pool->bins = calloc((Np+1) * sizeof(*pool->bins), 1);
    for (i = 0; i <= Np; i++)
        pool->bins[i].next = pool->bins[i].prev = &pool->bins[i];
    
    pool->addseg = greedy_pool_addseg;
    pool->insertseg = greedy_pool_insertseg;
    pool->getseg = greedy_pool_getseg;
    pool->write = greedy_int_write;
    pool->del = greedy_pool_del;
    pool->tail_utilization = greedy_tail_utilization;
    pool->next_segment = greedy_next_seg;
    pool->tail_segment = greedy_tail_segment;
    
    return pool;
}

static struct pool *do_select_first(struct ftl* ftl, int lba)
{
    return ftl->pools[0];
}

static struct pool *do_select_top_down(struct ftl* ftl, int lba)
{
    int i;
    struct segment *seg = ftl->map[lba].block;
    for (i = 1; i < ftl->npools; i++)
        if (seg && seg->pool == ftl->pools[i])
            return ftl->pools[i-1];
    return ftl->pools[0];
}

struct pool *pool_retval;
void return_pool(struct pool *pool)
{
    pool_retval = pool;
}

static struct pool *python_select_1_arg(struct ftl *ftl, int lba)
{
    PyObject *args = Py_BuildValue("(i)", lba);
    PyObject *result = PyEval_CallObject(ftl->get_input_pool_arg, args);
    if (PyErr_Occurred())
        longjmp(bailout_buf, 1);
    if (result != NULL) {
        Py_DECREF(result);
    }
    Py_DECREF(args);
    return pool_retval;
}

write_selector_t write_select_first = do_select_first;
write_selector_t write_select_top_down = do_select_top_down;
write_selector_t write_select_python = python_select_1_arg;

static struct pool *do_clean_select_first(struct ftl *ftl)
{
    return ftl->pools[0];
}
clean_selector_t clean_select_first = do_clean_select_first;

static struct pool *python_select_no_arg(struct ftl *ftl)
{
    PyObject *args = Py_BuildValue("()");
    PyObject *result = PyEval_CallObject(ftl->get_pool_to_clean_arg, args);
    if (PyErr_Occurred()) 
        longjmp(bailout_buf, 1);
    if (result != NULL) {
        Py_DECREF(result);
    }
    return pool_retval;
}

clean_selector_t clean_select_python = python_select_no_arg;
