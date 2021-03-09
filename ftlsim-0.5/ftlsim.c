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
#include <limits.h>

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

    fb->type = 0;
    fb->erase_counts = 0;

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

void do_segment_overwrite(struct segment *self, int page, int lba) //invalidate old lba page when new version of lba arrives
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
    blk->read_counts = 0;
    // for maintaining the hottest and coldest block
    if (blk == self->hottest) {
        self->hottest = self->second_hottest;
        self->second_hottest = NULL;
    } else if (blk == self->second_hottest) {
        self->second_hottest = NULL;
    }
//    else if (blk == self->coldest) {
//        self->coldest = self->second_coldest;
//        self->second_coldest = NULL;
//    } else if (blk == self->second_coldest) {
//        self->second_coldest = NULL;
//    }


}

struct segment *do_get_blk(struct ftl *self)
{
    if (self->free_list == NULL) {
        return NULL;
    }
    struct segment *start = self->free_list;
    struct segment *tail = start->next;
    if (tail != NULL) {
        while (tail->next != NULL){
            start = start->next;
            tail = tail->next;
        }
//        self->free_list = val->next;
        self->nfree--;
        start->next = NULL;
        return tail;
    }
    self->nfree--;
    self->free_list = start->next;
    return start;
}

static void check_new_segment(struct ftl *ftl, struct pool *pool)
{
    assert(pool->i <= pool->Np);
    if (pool->i >= pool->Np) {
        struct segment *b = do_get_blk(ftl);
        struct segment *blk = pool->frontier;
        pool->addseg(pool, b);
        // for maintaining the hottest and coldest block
        if (ftl->wl_activated == 0){
            return;
        }
        if (blk->type == 1){
            if (ftl->hottest == NULL) {
                ftl->hottest = blk;
            } else if (ftl->second_hottest != NULL) {
                if (ftl->hottest->erase_counts < blk->erase_counts) {
                    ftl->second_hottest = ftl->hottest;
                    ftl->hottest = blk;
                } else if (ftl->second_hottest->erase_counts < blk->erase_counts) {
                    ftl->second_hottest = blk;
                }
            } else {
                if (ftl->hottest->erase_counts < blk->erase_counts) {
                    ftl->second_hottest = ftl->hottest;
                    ftl->hottest = blk;
                } else {
                    ftl->second_hottest = blk;
                }
            }
        }
        else {
            if (ftl->hottest != NULL && blk->erase_counts >= ftl->hottest->erase_counts + ftl->wl_threshold){
                blk->type = 1;
            }
//            if (ftl->coldest == NULL) {
//                ftl->coldest = blk;
//            } else if (ftl->second_coldest != NULL) {
//                if (ftl->coldest->erase_counts > blk->erase_counts) {
//                    ftl->second_coldest = ftl->coldest;
//                    ftl->coldest = blk;
//                } else if (ftl->second_coldest->erase_counts > blk->erase_counts) {
//                    ftl->second_coldest = blk;
//                }
//            } else {
//                if (ftl->coldest->erase_counts > blk->erase_counts) {
//                    ftl->second_coldest = ftl->coldest;
//                    ftl->coldest = blk;
//                } else {
//                    ftl->second_coldest = blk;
//                }
//            }
        }
    }
}

void do_ftl_write(struct ftl *ftl, int addrs) {
    int i,j;
    int lba = addrs;
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
    check_new_segment(ftl, pool); // put one blk from ftl freelist to pool if pool.frontier is full

    /***GC***/
    int gc = 0;
    while (ftl->nfree < ftl->minfree) { //GC trigger test
        gc = 1;
        pool = ftl->get_pool_to_clean(ftl);
        struct segment *b = pool->getseg(pool); // get one block from pool to clean
        if (b == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "ftl: segment cleaning error");
            longjmp(bailout_buf, 1);
        }
        struct pool *next = pool->next_pool;
        if (pool == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "ftl: pool.next_pool = None");
            longjmp(bailout_buf, 1);
        }

        for (j = 0; j < b->Np; j++) // migrate valid pages
            if (b->lba[j] != -1) {
                next->write(ftl, next, b->lba[j]);
                check_new_segment(ftl, next);
            }
        if (ftl->wl_activated == 1) {
                b->erase_counts++;  // update erase counts
//                b->effective_ec++;  // update effective erase counts

        }
//        b->read_counts = 0;
        do_put_blk(ftl, b);
    }
    /***GC***/

    /***WL***/
    if (ftl->wl_threshold != 0 && gc == 1 && ftl->hottest != NULL) {
        if (ftl->wl_activated == 1 ) { // Greedy
            int max = ftl->hottest->erase_counts;
            struct segment *hottest = ftl->hottest;

            pool = ftl->get_pool_to_clean(ftl);
//            int max = INT_MIN;
            int min = INT_MAX;
//            struct segment *hottest = NULL;
            struct segment *coldest = NULL;
            for (j = pool->Np; j > 0; j--) { // find the hottest block and coldest block
                if (min < max - ftl->wl_threshold) {
                    break;
                }
                if (!list_empty(&pool->bins[j])) {
                    struct segment *b = (&pool->bins[j])->next;
//                    for (; b != (&pool->bins[j]); b = b->next) {
                        if (b->type == 0 && b->erase_counts < min) { // cold block
                            coldest = b;
                            min = b->erase_counts;
                        }

                        if (b->type == 1 && max - b->erase_counts > 2 * ftl->wl_threshold ) { // hot block
                            b->type = 0;
                        }
//
//                    }
                }

            }
//            int max = ftl->hottest->erase_counts;
//            int min = ftl->coldest->erase_counts;
//            struct segment *hottest = ftl->hottest;
//            struct segment *coldest = ftl->coldest;
//            printf("max = %d, min = %d\n", max, min);

            if ((max - min) > ftl->wl_threshold) { // WL trigger test
                ftl->wl_counts++;
                ftl->hottest = ftl->second_hottest;
                ftl->second_hottest = NULL;
//                ftl->coldest = ftl->second_coldest;
//                ftl->second_coldest = NULL;

                // hot-cold regulation
                hottest->type = 0;
                coldest->type = 1;
                hottest->read_counts = 0;

                // move data in the hottest block to a free unit
                list_rm(hottest);
                hottest->in_pool = 0;
                pool->pages_valid -= hottest->n_valid;
                pool->pages_invalid -= (pool->Np - hottest->n_valid);
                assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
                pool->length--;
                assert(pool->length >= 0);

                struct segment *free = do_get_blk(ftl); // choose one free block
                free->pool = pool;
                for (j = 0; j < hottest->Np; j++) {
                    if (hottest->lba[j] != -1) {
                        ftl->wl_writes++;
                        int tmp_lba = hottest->lba[j];
                        int tmp_page = ftl->map[tmp_lba].page_num;
                        do_segment_overwrite(hottest, tmp_page, tmp_lba);
                        do_segment_write(free, tmp_page, tmp_lba);
                    }
                }
                pool->insertseg(pool, free);


                // erase the hottest block
                hottest->erase_counts++;

                // cold data migration
                list_rm(coldest);
                coldest->in_pool = 0;
                pool->pages_valid -= coldest->n_valid;
                pool->pages_invalid -= (pool->Np - coldest->n_valid);
                assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
                pool->length--;
                assert(pool->length >= 0);

                for (j = 0; j < coldest->Np; j++) {
                    if (coldest->lba[j] != -1) {
                        ftl->wl_writes++;
                        int tmp_lba = coldest->lba[j];
                        int tmp_page = ftl->map[tmp_lba].page_num;
                        do_segment_overwrite(coldest, tmp_page, tmp_lba);
                        do_segment_write(hottest, tmp_page, tmp_lba);
                    }
                }
                pool->insertseg(pool, hottest);

                // erase the coldest block and put it to freelist
                coldest->erase_counts++;
                do_put_blk(ftl, coldest);

                // reset effective erase count
//                hottest->effective_ec = 0;
//                coldest->effective_ec = 0;

            }

        } else { // LRU
//                ftl->int_writes += ftl->Np;
        }
     }
    /***WL***/
}

void do_ftl_read(struct ftl *ftl, int addrs) {
    int i, j;
    int lba = addrs;
    if (lba == -1)
        return;
    assert(lba >= 0 && lba < ftl->T * ftl->Np);
    struct segment *b = ftl->map[lba].block;
//    printf("magic:%d\n", b->magic);
//    int page = ftl->map[lba].page_num;
    b->read_counts += 1;
    ftl->ext_reads += 1;
    //read reclaim checking
    if (b->read_counts > ftl->rr_threshold) {
        ftl->rr_counts += 1;
        ftl->rr_writes += b->n_valid;
        b->read_counts = 0;
        // swap block info
        b->erase_counts += 1;
//        ftl->free_list->erase_counts += 1;
        int tmp = b->erase_counts;
        b->erase_counts = ftl->free_list->erase_counts;
        ftl->free_list->erase_counts = tmp;

//        // step 1 : get input pool
//        struct pool *pool = NULL;
//        if (ftl->get_input_pool)
//            pool = ftl->get_input_pool(ftl, lba);
//        if (pool == NULL) {
//            PyErr_SetString(PyExc_RuntimeError, "ftl: input_pool error");
//            longjmp(bailout_buf, 1);
//        }
//        // step 2 : get one free unit
//        struct segment *free = do_get_blk(ftl);
//        free->pool = pool;
//        // step 3 : migrate data
//        printf("rr step3\n");
//        list_rm(b);
//        b->in_pool = 0;
//        pool->pages_valid -= b->n_valid;
//        pool->pages_invalid -= (pool->Np - b->n_valid);
//        assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
//        pool->length--;
//        assert(pool->length >= 0);
//
//        for (j = 0; j < b->Np; j++)
//            if (b->lba[j] != -1) {
//                ftl->rr_writes++;
//                int tmp_lba = b->lba[j];
//                int tmp_page = ftl->map[tmp_lba].page_num;
//                do_segment_overwrite(b, tmp_page, tmp_lba);
//                do_segment_write(free, tmp_page, tmp_lba);
//            }
//        pool->insertseg(pool, free);
//        // step 4 : erase the RR-ed block
//        b->read_counts = 0;
//        b->erase_counts += 1;
//        do_put_blk(ftl, b);
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
        check_new_segment(ftl, pool); // put one blk from ftl freelist to pool if pool.frontier is full

        int gc = 0;
        while (ftl->nfree < ftl->minfree) { //GC trigger test
            gc = 1;
            pool = ftl->get_pool_to_clean(ftl);
            struct segment *b = pool->getseg(pool); // get one block from pool to clean
            if (b == NULL) {
                PyErr_SetString(PyExc_RuntimeError, "ftl: segment cleaning error");
                longjmp(bailout_buf, 1);
            }
            struct pool *next = pool->next_pool;
            if (pool == NULL) {
                PyErr_SetString(PyExc_RuntimeError, "ftl: pool.next_pool = None");
                longjmp(bailout_buf, 1);
            }

            for (j = 0; j < b->Np; j++) // migrate valid pages
                if (b->lba[j] != -1) {
                    next->write(ftl, next, b->lba[j]);
                    check_new_segment(ftl, next);
                }
            if (ftl->wl_activated == 1) {
                b->erase_counts++;  // update erase counts
//                b->effective_ec++;  // update effective erase counts
            }
            do_put_blk(ftl, b);
        }

        /***WL***/
        if (ftl->wl_threshold != 0 && gc == 1 && ftl->hottest != NULL && ftl->coldest != NULL) {
            if (ftl->wl_activated == 1 ) { // Greedy
                pool = ftl->get_pool_to_clean(ftl);
//                int max = INT_MIN;
//                int min = INT_MAX;
//                int hot_min = INT_MAX;
//                int cold_eec_max = INT_MIN;
//                int hot_eec_min = INT_MAX;
//                struct segment *hottest = NULL;
//                struct segment *coldest = NULL;
//                struct segment *hmin = NULL;
//                struct segment *cold_eec = NULL;
//                struct segment *hot_eec = NULL;
//                for (j = 0; j <= pool->Np; j++) { // find the hottest block and coldest block
//                    if (!list_empty(&pool->bins[j])) {
//                        struct segment *b = (&pool->bins[j])->next;
//                        for (; b != (&pool->bins[j]); b = b->next) {
//                            if (b->type == 0 && b->erase_counts < min ) { // cold block
//                                min = b->erase_counts;
//                                coldest = b;
//                            }
//                            if (b->type == 0 && b->effective_ec > cold_eec_max ) { // cold pool resize
//                                cold_eec_max = b->effective_ec;
//                                cold_eec = b;
//                            }
//                            if (b->type == 1 && b->erase_counts > max ) { // hot block
//                                max = b->erase_counts;
//                                hottest = b;
//                            }
//                            if (b->type == 1 && b->erase_counts < hot_min ) { // hot pool resize
//                                hot_min = b->erase_counts;
//                                hmin = b;
//                            }
//                            if (b->type == 1 && b->effective_ec < hot_eec_min ) { // cold pool resize
//                                hot_eec_min = b->effective_ec;
//                                hot_eec = b;
//                            }
//                        }
//                    }
//                }
//                if (hottest == NULL || coldest == NULL || hmin == NULL || cold_eec == NULL || hot_eec == NULL) {
//                    continue;
//                }
                int max = ftl->hottest->erase_counts;
                int min = ftl->coldest->erase_counts;
                struct segment *hottest = ftl->hottest;
                struct segment *coldest = ftl->coldest;
                if ((max - min) > ftl->wl_threshold) { // WL trigger test
                    ftl->wl_counts++;
                    ftl->hottest = ftl->second_hottest;
                    ftl->second_hottest = NULL;
                    ftl->coldest = ftl->second_coldest;
                    ftl->second_coldest = NULL;

                    // hot-cold regulation
                    hottest->type = 0;
                    coldest->type = 1;
                    ftl->hottest->read_counts = 0;

                    // move data in the hottest block to a free unit
                    list_rm(hottest);
                    hottest->in_pool = 0;
                    pool->pages_valid -= hottest->n_valid;
                    pool->pages_invalid -= (pool->Np - hottest->n_valid);
                    assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
                    pool->length--;
                    assert(pool->length >= 0);

                    struct segment *free = do_get_blk(ftl); // choose one free block
                    free->pool = pool;
                    for (j = 0; j < hottest->Np; j++) {
                        if (hottest->lba[j] != -1) {
                            ftl->wl_writes++;
                            int tmp_lba = hottest->lba[j];
                            int tmp_page = ftl->map[tmp_lba].page_num;
                            do_segment_overwrite(hottest, tmp_page, tmp_lba);
                            do_segment_write(free, tmp_page, tmp_lba);
                        }
                    }
                    pool->insertseg(pool, free);


                    // erase the hottest block
                    hottest->erase_counts++;

                    // cold data migration
                    list_rm(coldest);
                    coldest->in_pool = 0;
                    pool->pages_valid -= coldest->n_valid;
                    pool->pages_invalid -= (pool->Np - coldest->n_valid);
                    assert(pool->pages_valid >= 0 && pool->pages_invalid >= 0);
                    pool->length--;
                    assert(pool->length >= 0);

                    for (j = 0; j < coldest->Np; j++) {
                        if (coldest->lba[j] != -1) {
                            ftl->wl_writes++;
                            int tmp_lba = coldest->lba[j];
                            int tmp_page = ftl->map[tmp_lba].page_num;
                            do_segment_overwrite(coldest, tmp_page, tmp_lba);
                            do_segment_write(hottest, tmp_page, tmp_lba);
                        }
                    }
                    pool->insertseg(pool, hottest);

                    // erase the coldest block and put it to freelist
                    coldest->erase_counts++;
                    do_put_blk(ftl, coldest);

                    // reset effective erase count
//                    hottest->effective_ec = 0;
//                    coldest->effective_ec = 0;

                }
                
                // cold pool resize
//               if (cold_eec_max - hot_eec_min > ftl->wl_threshold) {
//                   cold_eec->type = 1;
//               }

//              //  hot pool resize
//               if (max - hot_min > 2 * ftl->wl_threshold) {
//                   hmin->type = 0;
//               }
            } else { // LRU
//                ftl->int_writes += ftl->Np;
            }
        }
        /***WL***/
//        printf("count = %d\n", i);
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
//    pool->frontier->next->next->prev = blk; // add this line for wl
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
//    ftl->wl_activated = 1;
    /***wear_leveling***/
//    if (ftl->ext_writes > 0 && ftl->ext_writes % ftl->wl_threshold == 0) {
//        ftl->wl_activated = 1;
//        ftl->wl_counts++;
//    }
    /***wear_leveling***/
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
