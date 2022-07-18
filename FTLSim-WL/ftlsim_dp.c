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

#include<unistd.h>
#include<fcntl.h>
#include <sys/types.h>

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

    fb->erase_counts = 0;
    fb->effective_ec = 0;

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
    ftl->shrinkable = 0;

    //heap
    printf ("FTL T = %d\n", ftl->T);
//    printf ("FTL minfree = %d\n", ftl->minfree);

    ftl->cold_ec_min  = init_ecminheap(T+T/2);
    ftl->hot_ec_min   = init_ecminheap(T+T/2);
    ftl->hot_ec_max   = init_ecmaxheap(T+T/2);
    ftl->hot_eec_min  = init_eecminheap(T+T/2);
    ftl->cold_eec_max = init_eecmaxheap(T+T/2);
    return ftl;
}

void do_ftl_build_heap(struct ftl *ftl){
    ftl->cold_ec_min->size = 0;
    ftl->hot_ec_min->size = 0;
    ftl->hot_ec_max->size = 0;
    ftl->hot_eec_min->size = 0;
    ftl->cold_eec_max->size = 0;
    struct pool *pool = ftl->get_pool_to_clean(ftl);
    if (pool->frontier->type == 1) {
        insert_ecmaxheap(ftl->hot_ec_max, pool->frontier);
        insert_ecminheap(ftl->hot_ec_min, pool->frontier);
        insert_eecminheap(ftl->hot_eec_min, pool->frontier);
            ///***Debug: Start***///
//            if (ftl->hot_ec_max->arr[pool->frontier->ecmax_index] != pool->frontier) {
//                    printf("BD1 here: %d\n");
//                    exit(-1);
//            }
//            if (ftl->hot_ec_min->arr[pool->frontier->ecmin_index] != pool->frontier) {
//                    printf("BD2 here: %d\n");
//                    exit(-1);
//            }
//            if (ftl->hot_eec_min->arr[pool->frontier->eecmin_index] != pool->frontier) {
//                    printf("BD3 here: %d\n");
//                    exit(-1);
//            }
            ///***Debug: End***///
    } else {
        insert_ecminheap(ftl->cold_ec_min, pool->frontier);
        insert_eecmaxheap(ftl->cold_eec_max, pool->frontier);
            ///***Debug: Start***///
//            if (ftl->cold_ec_min->arr[pool->frontier->ecmin_index] != pool->frontier) {
//                    printf("BD4 here: %d\n");
//                    exit(-1);
//            }
//            if (ftl->cold_eec_max->arr[pool->frontier->eecmax_index] != pool->frontier) {
//                    printf("BD5 here: %d\n");
//                    exit(-1);
//            }
            ///***Debug: End***///
    }

    struct segment *tmp = ftl->free_list;
    int free = ftl->nfree;
    while (free > 0) {
        if (tmp->type == 1) {
            insert_ecmaxheap(ftl->hot_ec_max, tmp);
            insert_ecminheap(ftl->hot_ec_min, tmp);
            insert_eecminheap(ftl->hot_eec_min, tmp);
                ///***Debug: Start***///
//                if (ftl->hot_ec_max->arr[tmp->ecmax_index] != tmp) {
//                        printf("BD6 here: %d\n");
//                        exit(-1);
//                }
//                if (ftl->hot_ec_min->arr[tmp->ecmin_index] != tmp) {
//                        printf("BD7 here: %d\n");
//                        exit(-1);
//                }
//                if (ftl->hot_eec_min->arr[tmp->eecmin_index] != tmp) {
//                        printf("BD8 here: %d\n");
//                        exit(-1);
//                }
                ///***Debug: End***///
        } else {
            insert_ecminheap(ftl->cold_ec_min, tmp);
            insert_eecmaxheap(ftl->cold_eec_max, tmp);
                ///***Debug: Start***///
//                if (ftl->cold_ec_min->arr[tmp->ecmin_index] != tmp) {
//                        printf("BD9 here: %d\n");
//                        exit(-1);
//                }
//                if (ftl->cold_eec_max->arr[tmp->eecmax_index] != tmp) {
//                        printf("BD10 here: %d\n");
//                        exit(-1);
//                }
                ///***Debug: End***///
        }
        tmp = tmp->next;
        free--;
    }
    int j = 0;
    for (j = 0; j <= pool->Np; j++) {
        tmp = (&pool->bins[j])->next;
        for (; tmp != (&pool->bins[j]); tmp = tmp->next) {
            if (tmp->type == 1 ) { // hot block
                insert_ecmaxheap(ftl->hot_ec_max, tmp);
                insert_ecminheap(ftl->hot_ec_min, tmp);
                insert_eecminheap(ftl->hot_eec_min, tmp);
                    ///***Debug: Start***///
//                    if (ftl->hot_ec_max->arr[tmp->ecmax_index] != tmp) {
//                            printf("BD11 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_ec_min->arr[tmp->ecmin_index] != tmp) {
//                            printf("BD12 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_eec_min->arr[tmp->eecmin_index] != tmp) {
//                            printf("BD13 here: %d\n");
//                            exit(-1);
//                    }
                    ///***Debug: End***///
            } else { // cold block
                insert_ecminheap(ftl->cold_ec_min, tmp);
                insert_eecmaxheap(ftl->cold_eec_max, tmp);
                    ///***Debug: Start***///
//                    if (ftl->cold_ec_min->arr[tmp->ecmin_index] != tmp) {
//                            printf("BD14 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->cold_eec_max->arr[tmp->eecmax_index] != tmp) {
//                            printf("BD15 here: %d\n");
//                            exit(-1);
//                    }
                    ///***Debug: End***///
            }
        }

    }

    printf("heap build complete! Size,Capacity:\n");
    printf("hot_ec_max: %d,%d\n", ftl->hot_ec_max->size,ftl->hot_ec_max->capacity);
    printf("hot_ec_min: %d,%d\n", ftl->hot_ec_min->size,ftl->hot_ec_min->capacity);
    printf("hot_eec_min: %d,%d\n", ftl->hot_eec_min->size,ftl->hot_eec_min->capacity);
    printf("cold_ec_min: %d,%d\n", ftl->cold_ec_min->size,ftl->cold_ec_min->capacity);
    printf("cold_eec_max: %d,%d\n", ftl->cold_eec_max->size,ftl->cold_eec_max->capacity);

}

void ftl_del(struct ftl *ftl)
{
    struct segment *b;
    ftl->magic = 0;
    while ((b = do_get_blk(ftl)) != NULL)
        segment_del(b);
    free(ftl->map);
    //heap
    free_ecminheap(ftl->cold_ec_min);
    free_ecminheap(ftl->hot_ec_min);
    free_ecmaxheap(ftl->hot_ec_max);
    free_eecminheap(ftl->hot_eec_min);
    free_eecmaxheap(ftl->cold_eec_max);
    free(ftl);
}

void do_put_blk(struct ftl *self, struct segment *blk)
{
    blk->next = self->free_list;
    self->free_list = blk;
    self->nfree++;
    blk->read_counts = 0;
    // for maintaining the hottest and coldest block
//    if (blk == self->hottest) {
//        self->hottest = self->second_hottest;
//        self->second_hottest = NULL;
//    } else if (blk == self->second_hottest) {
//        self->second_hottest = NULL;
//    }
}

struct segment *do_get_blk(struct ftl *self)
{
    if (self->nfree > 50) {
        struct segment *val = self->free_list;
        if (val != NULL) {
            self->free_list = val->next;
            self->nfree--;
        }
        return val;
    }

    // FIFO
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
//        struct segment *blk = pool->frontier;
        pool->addseg(pool, b);
    }
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


int do_ftl_write(struct ftl *ftl, int addrs) {
    int i,j;
    int lba = addrs;
    if (lba == -1)
        return -1;
    assert(lba >= 0 && lba < ftl->T * ftl->Np);
    // record host footprint
    ftl->map[lba].host = 1;
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
    if (ftl->nfree == 0 && ftl->shrinkable == 0) {
        return 1;
    }
    check_new_segment(ftl, pool); // put one blk from ftl freelist to pool if pool.frontier is full

//        int gc = 0;
    // count TBW - count in host layer for this mode
//    if (ftl->ext_writes == (ftl->T * ftl->Np)) {
//        ftl->ext_writes = 0;
//        ftl->TBW++;
//    }
    while (ftl->nfree < ftl->minfree) { //GC trigger test
//            gc = 1;
        pool = ftl->get_pool_to_clean(ftl);
        struct segment *b = pool->getseg(pool); // get one block from pool to clean
        if (b == NULL) {
//                PyErr_SetString(PyExc_RuntimeError, "ftl: segment cleaning error");
//                longjmp(bailout_buf, 1);
//            printf("Cleaning from full valid blocks, bad blocks: %d\n", ftl->bad_blocks);
            return 1;
        }
        struct pool *next = pool->next_pool;
        if (pool == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "ftl: pool.next_pool = None");
            longjmp(bailout_buf, 1);
        }

        for (j = 0; j < b->Np; j++) { // migrate valid pages
            if (b->lba[j] != -1) {
                next->write(ftl, next, b->lba[j]);
                check_new_segment(ftl, next);
            }
        }
//            if (ftl->wl_activated == 1) {
        b->erase_counts++;  // update erase counts
        b->effective_ec++;  // update effective erase counts
        b->gc_counts++;
//            printf("GC: block_id=%d block_type=%d block_gc=%d block_erase=%d block_eec=%d\n", b->id, b->type, b->gc_counts, b->erase_counts, b->effective_ec);
//            }

        int map_out = 0;
        if (b->erase_counts >= ftl->endurance) {
            map_out = 1;
            if (ftl->wl_activated == 1) {
                if (b->type == 1) {
                    ecmax_delete_element(ftl->hot_ec_max, b->ecmax_index);
                    ecmin_delete_element(ftl->hot_ec_min, b->ecmin_index);
                    eecmin_delete_element(ftl->hot_eec_min, b->eecmin_index);
                } else {
                    ecmin_delete_element(ftl->cold_ec_min, b->ecmin_index);
                    eecmax_delete_element(ftl->cold_eec_max, b->eecmax_index);
                }
            }
            ftl->bad_blocks++;
//            printf("Mapped out bad block_id: %d. Total bad blocks: %d.\n", b->id, ftl->bad_blocks);
//            segment_del(b);
            if (ftl->nfree == 1 && ftl->shrinkable != 0) {
//                printf("Terminated: freelist empty!\n");
                int bounds = ftl->U * ftl->Np;
                for (i = 0; i < bounds; i++){
                    if (ftl->map[i].host != 1) { // shrink non-host data
                        do_segment_overwrite(ftl->map[i].block, ftl->map[i].page_num, i);
                        ftl->map[i].block = NULL;
                        ftl->shrinkable = 0;
                    }
                }
//                return 1;
            }
            if (ftl->shrinkable != 0 && ftl->bad_blocks >= (ftl->T - ftl->U) ){
//                printf("Terminated: process complete!\n");
                int bounds = ftl->U * ftl->Np;
                for (i = 0; i < bounds; i++){
                    if (ftl->map[i].host != 1) { // shrink non-host data
                        do_segment_overwrite(ftl->map[i].block, ftl->map[i].page_num, i);
                        ftl->map[i].block = NULL;
                        ftl->shrinkable = 0;
                    }
                }
//                return 1;
            }
            if (ftl->nfree == 0 && ftl->shrinkable == 0) {
                return 1;
            }
        } else {
            do_put_blk(ftl, b);
        }
        // heap update
        if (ftl->wl_activated == 1 && map_out == 0) {
            if (b->type == 1) {
                ecmax_addone_check(ftl->hot_ec_max, b->ecmax_index);
                ecmin_addone_check(ftl->hot_ec_min, b->ecmin_index);
                eecmin_addone_check(ftl->hot_eec_min, b->eecmin_index);
                ///***Debug: Start***///
//                    if (ftl->hot_ec_max->arr[b->ecmax_index] != b) {
//                            printf("GC1 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_ec_min->arr[b->ecmin_index] != b) {
//                            printf("GC2 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_eec_min->arr[b->eecmin_index] != b) {
//                            printf("GC3 here: %d\n");
//                            exit(-1);
//                    }
                ///***Debug: End***///
            } else {
                ecmin_addone_check(ftl->cold_ec_min, b->ecmin_index);
                eecmax_addone_check(ftl->cold_eec_max, b->eecmax_index);
                ///***Debug: Start***///
//                    if (ftl->cold_ec_min->arr[b->ecmin_index] != b) {
//                            printf("GC4 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->cold_eec_max->arr[b->eecmax_index] != b) {
//                            printf("GC5 here: %d\n");
//                            exit(-1);
//                    }
                ///***Debug: End***///
            }
        }
    }

    /***WL***/
//        if (ftl->wl_threshold != 0 && gc == 1) {
    if (ftl->wl_threshold != 0 ) {
        if (ftl->wl_activated == 1 ) { // Greedy
            struct segment *hottest = NULL;
            struct segment *coldest = NULL;

            // dirty swap
            if (ftl->wl_ds == 1) {
                hottest = get_ecmax(ftl->hot_ec_max);
                coldest = get_ecmin(ftl->cold_ec_min);
                if (hottest == NULL || coldest == NULL) {
    //                printf("DS Attention: Some pools are empty!\n");
                }

                // dirty swap
                else if ((hottest->erase_counts - coldest->erase_counts) > ftl->wl_threshold) { // WL trigger test
                    ftl->wl_counts++;
    //                printf("DS%d: hottest_id=%d hottest_erase=%d coldest_id=%d coldest_erase=%d\n", ftl->wl_counts, hottest->id, hottest->erase_counts, coldest->id, coldest->erase_counts);
    //                printf("DS%d: hottest_id=%d goes into cold pool\n", ftl->wl_counts, hottest->id);
    //                printf("DS%d: coldest_id=%d goes into hot pool\n", ftl->wl_counts, coldest->id);

                    // hot-cold regulation
                    hottest->type = 0;
                    coldest->type = 1;
                    hottest->read_counts = 0;
                    // reset effective erase count
                    hottest->effective_ec = 0;
                    coldest->effective_ec = 0;

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
                    hottest->in_pool = 1;
                    coldest->in_pool = 0;
                    free->in_pool = 1;

                    //update heap
                    ecmax_delete_element(ftl->hot_ec_max, hottest->ecmax_index);
                    ecmin_delete_element(ftl->hot_ec_min, hottest->ecmin_index);
                    eecmin_delete_element(ftl->hot_eec_min, hottest->eecmin_index);
                    insert_ecminheap(ftl->cold_ec_min, hottest);
                    insert_eecmaxheap(ftl->cold_eec_max, hottest);
                    ///***Debug: Start***///
    //                    if (ftl->cold_ec_min->arr[hottest->ecmin_index] != hottest) {
    //                        printf("DS1 here: %d\n");
    //                        exit(-1);
    //                    }
    //                    if (ftl->cold_eec_max->arr[hottest->eecmax_index] != hottest) {
    //                        printf("DS2 here: %d\n");
    //                        exit(-1);
    //                    }
                    ///***Debug: End***///

                    ecmin_delete_element(ftl->cold_ec_min, coldest->ecmin_index);
                    eecmax_delete_element(ftl->cold_eec_max, coldest->eecmax_index);
                    insert_ecmaxheap(ftl->hot_ec_max, coldest);
                    insert_ecminheap(ftl->hot_ec_min, coldest);
                    insert_eecminheap(ftl->hot_eec_min, coldest);
                    ///***Debug: Start***///
    //                    if (ftl->hot_ec_max->arr[coldest->ecmax_index] != coldest) {
    //                        printf("DS3 here: %d\n");
    //                        exit(-1);
    //                    }
    //                    if (ftl->hot_ec_min->arr[coldest->ecmin_index] != coldest) {
    //                        printf("DS4 here: %d\n");
    //                        exit(-1);
    //                    }
    //                    if (ftl->hot_eec_min->arr[coldest->eecmin_index] != coldest) {
    //                        printf("DS5 here: %d\n");
    //                        exit(-1);
    //                    }
                    ///***Debug: End***///
                }
            }

            // cold pool resize
            hottest = NULL;
            coldest = NULL;
//                hottest = get_eecmax(ftl->cold_eec_max);
//                coldest = get_eecmin(ftl->hot_eec_min);
            hottest = ftl->cold_eec_max->size > 0 ? ftl->cold_eec_max->arr[0] : NULL;
            coldest = ftl->hot_eec_min->size > 0 ? ftl->hot_eec_min->arr[0] : NULL;
            if (hottest == NULL || coldest == NULL) {
//                printf("CPR Attention: Some pools are empty!\n");
            }
            else if ((hottest->effective_ec - coldest->effective_ec) > ftl->wl_threshold) {
                ftl->cpr++;
//                printf("CPR%d: block_id=%d goes into hot pool\n", ftl->cpr, hottest->id);
                ///***Debug: Start***///
//                    if (ftl->cold_eec_max->arr[hottest->eecmax_index] != hottest) {
//                        printf("HIndex error\n");
//                        exit(-1);
//                    }
//                    if (ftl->hot_eec_min->arr[coldest->eecmin_index] != coldest) {
//                        printf("CIndex error\n");
//                        exit(-1);
//                    }
                ///***Debug: End***///
//                    printf("In CPR: EEC[H+(cold)]=%d, EEC[H-(hot)]=%d\n", hottest->effective_ec, coldest->effective_ec);
                hottest->type = 1;
                //update heap
                ecmin_delete_element(ftl->cold_ec_min, hottest->ecmin_index);
                eecmax_delete_element(ftl->cold_eec_max, hottest->eecmax_index);
                insert_ecmaxheap(ftl->hot_ec_max, hottest);
                insert_ecminheap(ftl->hot_ec_min, hottest);
                insert_eecminheap(ftl->hot_eec_min, hottest);
                ///***Debug: Start***///
//                    if (ftl->hot_ec_max->arr[hottest->ecmax_index] != hottest) {
//                            printf("CPR1 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_ec_min->arr[hottest->ecmin_index] != hottest) {
//                            printf("CPR2 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_eec_min->arr[hottest->eecmin_index] != hottest) {
//                            printf("CPR3 here: %d\n");
//                            exit(-1);
//                    }
                ///***Debug: End***///
            }
            // hot pool resize
            hottest = NULL;
            coldest = NULL;
//                hottest = get_ecmax(ftl->hot_ec_max);
//                coldest = get_ecmin(ftl->hot_ec_min);
            hottest = ftl->hot_ec_max->size > 0 ? ftl->hot_ec_max->arr[0] : NULL;
            coldest = ftl->hot_ec_min->size > 0 ? ftl->hot_ec_min->arr[0] : NULL;
            if (hottest == NULL || coldest == NULL) {
//                printf("HPR Attention: Some pools are empty!\n");
            }
            else if ((hottest->erase_counts - coldest->erase_counts) > 2 * ftl->wl_threshold) {
                ftl->hpr++;
//                printf("HPR%d: block_id=%d goes into cold pool\n", ftl->hpr, coldest->id);
                coldest->type = 0;
                //update heap
                ecmax_delete_element(ftl->hot_ec_max, coldest->ecmax_index);
                ecmin_delete_element(ftl->hot_ec_min, coldest->ecmin_index);
                eecmin_delete_element(ftl->hot_eec_min, coldest->eecmin_index);
                insert_ecminheap(ftl->cold_ec_min, coldest);
                insert_eecmaxheap(ftl->cold_eec_max, coldest);
            }

        }
    }
    /***WL***/
    return 0;
}


void do_ftl_run(struct ftl *ftl, struct getaddr *addrs, int count)
{
//    if (count == 100) { // TBW mode, log triggered
//        char path[100];
//        int pid = getpid();
//        snprintf(path, sizeof(path), "/home/zjiao04/ftlsim/pid_%d.log", pid);
//        int fd = open(path, O_RDWR|O_CREAT|O_APPEND, 0666);
//        dup2(fd, STDOUT_FILENO);
//        dup2(fd, STDERR_FILENO);
//        printf("fd : %d\n", fd);
//        printf("pid : %d\n", pid);
//    }

    int i, j;
    if (setjmp(bailout_buf) != 0) {
        err_occurred = 1;
        return;
    }
    for (i = 0; i < count; i++) {
//        if (count == 100) {
//            i = 0;
//        }
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

//        int gc = 0;
        // count TBW
//        if (ftl->ext_writes == (ftl->T * ftl->Np)) {
//            ftl->ext_writes = 0;
//            ftl->TBW++;
//        }
        while (ftl->nfree < ftl->minfree) { //GC trigger test
//            gc = 1;
            pool = ftl->get_pool_to_clean(ftl);
            struct segment *b = pool->getseg(pool); // get one block from pool to clean
            if (b == NULL) {
//                PyErr_SetString(PyExc_RuntimeError, "ftl: segment cleaning error");
//                longjmp(bailout_buf, 1);
//                printf("Cleaning from full valid blocks, bad blocks: %d\n", ftl->bad_blocks);
                return;
            }
            struct pool *next = pool->next_pool;
            if (pool == NULL) {
                PyErr_SetString(PyExc_RuntimeError, "ftl: pool.next_pool = None");
                longjmp(bailout_buf, 1);
            }

            for (j = 0; j < b->Np; j++) {// migrate valid pages
                if (b->lba[j] != -1) {
                    next->write(ftl, next, b->lba[j]);
                    check_new_segment(ftl, next);
                }
            }
//            if (ftl->wl_activated == 1) {
            b->erase_counts++;  // update erase counts
            b->effective_ec++;  // update effective erase counts
            b->gc_counts++;
//                printf("GC: block_id=%d block_type=%d block_gc=%d block_erase=%d block_eec=%d\n", b->id, b->type, b->gc_counts, b->erase_counts, b->effective_ec);
//            }

            int map_out = 0;
            if (b->erase_counts >= ftl->endurance) {
                map_out = 1;
                if (ftl->wl_activated == 1) {
                    if (b->type == 1) {
                        ecmax_delete_element(ftl->hot_ec_max, b->ecmax_index);
                        ecmin_delete_element(ftl->hot_ec_min, b->ecmin_index);
                        eecmin_delete_element(ftl->hot_eec_min, b->eecmin_index);
                    } else {
                        ecmin_delete_element(ftl->cold_ec_min, b->ecmin_index);
                        eecmax_delete_element(ftl->cold_eec_max, b->eecmax_index);
                    }
                }
                ftl->bad_blocks++;
//                printf("Mapped out bad block_id: %d. Total bad blocks: %d.\n", b->id, ftl->bad_blocks);
//                segment_del(b);
                if (ftl->nfree == 0) {
//                    printf("Terminated: freelist empty!\n");
                    return;
                }
                if (ftl->bad_blocks >= (ftl->T - ftl->U)){
//                    printf("Terminated: process complete!\n");
                    return;
                }
            } else {
                do_put_blk(ftl, b);
            }
            // heap update
            if (ftl->wl_activated == 1 && map_out == 0) {
                if (b->type == 1) {
                    ecmax_addone_check(ftl->hot_ec_max, b->ecmax_index);
                    ecmin_addone_check(ftl->hot_ec_min, b->ecmin_index);
                    eecmin_addone_check(ftl->hot_eec_min, b->eecmin_index);
                    ///***Debug: Start***///
//                    if (ftl->hot_ec_max->arr[b->ecmax_index] != b) {
//                            printf("GC1 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_ec_min->arr[b->ecmin_index] != b) {
//                            printf("GC2 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->hot_eec_min->arr[b->eecmin_index] != b) {
//                            printf("GC3 here: %d\n");
//                            exit(-1);
//                    }
                    ///***Debug: End***///
                } else {
                    ecmin_addone_check(ftl->cold_ec_min, b->ecmin_index);
                    eecmax_addone_check(ftl->cold_eec_max, b->eecmax_index);
                    ///***Debug: Start***///
//                    if (ftl->cold_ec_min->arr[b->ecmin_index] != b) {
//                            printf("GC4 here: %d\n");
//                            exit(-1);
//                    }
//                    if (ftl->cold_eec_max->arr[b->eecmax_index] != b) {
//                            printf("GC5 here: %d\n");
//                            exit(-1);
//                    }
                    ///***Debug: End***///
                }
            }
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
//    updated out for TBW test
    if (i == pool->Np) {
//        PyErr_SetString(PyExc_RuntimeError, "ftl: greedy: pool full");
//        longjmp(bailout_buf, 1);
        return NULL;
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
    if (fb == NULL) {
        printf("fb null exit\n");
        exit(-1);
    }
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

/***general methods***/
int parent(int i) {
    // Get the index of the parent
    return (i - 1) / 2;
}

int left_child(int i) {
    return (2*i + 1);
}

int right_child(int i) {
    return (2*i + 2);
}
/***general methods***/


/***EC min heap***/
//typedef struct ECMinHeap ECMinHeap;
//struct ECMinHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_ecmin(ECMinHeap* heap) {
    // Return the root node element,
    // since that's the minimum
//    return heap->arr[0];
    struct segment *min = NULL;
//    for (int i = 0; i <= 6; i++){
    int last = 0;
    for (int i = 0; i < heap->size; i++){
        if (i > heap->size - 1) {
            return min == NULL ? NULL : min;
        }
        if ((heap->arr[i]->in_pool == 1 && min == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->erase_counts < min->erase_counts)) {
            min = heap->arr[i];
        }
//        if (i == 0 || i == 2 || i == 6) {
        if (i == last) {
            if (min != NULL) return min;
            last = right_child(last);
        }
    }
    return NULL;
}

ECMinHeap* init_ecminheap(int capacity) {
    ECMinHeap* minheap = (ECMinHeap*) calloc (1, sizeof(ECMinHeap));
    minheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    minheap->capacity = capacity;
    minheap->size = 0;
    return minheap;
}

ECMinHeap* insert_ecminheap(ECMinHeap* heap, struct segment* element) {
    // Inserts an element to the min heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is lesser than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the min heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "ECMinHeap: Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->ecmin_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts > heap->arr[curr]->erase_counts) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmin_index;
        heap->arr[parent(curr)]->ecmin_index = heap->arr[curr]->ecmin_index;
        heap->arr[curr]->ecmin_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

ECMinHeap* ecminheapify(ECMinHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the min-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // Variable to get the smallest element of the subtree
    // of an element an index
    int smallest = index;

    // If the left child is smaller than this element, it is
    // the smallest
    if (left < heap->size && heap->arr[left]->erase_counts < heap->arr[index]->erase_counts)
        smallest = left;

    // Similarly for the right, but we are updating the smallest element
    // so that it will definitely give the least element of the subtree
    if (right < heap->size && heap->arr[right]->erase_counts < heap->arr[smallest]->erase_counts)
        smallest = right;

    // Now if the current element is not the smallest,
    // swap with the current element. The min heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (smallest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[smallest];
        heap->arr[smallest] = temp;
        //swap index
        int tmp = heap->arr[index]->ecmin_index;
        heap->arr[index]->ecmin_index = heap->arr[smallest]->ecmin_index;
        heap->arr[smallest]->ecmin_index = tmp;

        heap = ecminheapify(heap, smallest);
    }

    return heap;
}

ECMinHeap* delete_ecminimum(ECMinHeap* heap) {
    // Deletes the minimum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->ecmin_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the min-heap
    // property
    heap = ecminheapify(heap, 0);
    return heap;
}

ECMinHeap* ecmin_delete_element(ECMinHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's lesser than the current root
    int cur_erase_counts = heap->arr[index]->erase_counts;
    heap->arr[index]->erase_counts = -1;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts > heap->arr[curr]->erase_counts) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmin_index;
        heap->arr[parent(curr)]->ecmin_index = heap->arr[curr]->ecmin_index;
        heap->arr[curr]->ecmin_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->erase_counts = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_ecminimum(heap);
    return heap;
}

ECMinHeap* ecmin_addone_check(ECMinHeap* heap, int index) {
    return ecminheapify(heap, index);
}

void print_ecminheap(ECMinHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("ECMin Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d -> ", heap->arr[i]->erase_counts, heap->arr[i]->ecmin_index);
    }
    printf("\n");
}
void free_ecminheap(ECMinHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EC min heap***/

/***EC max heap***/
//typedef struct ECMaxHeap ECMaxHeap;
//struct ECMaxHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_ecmax(ECMaxHeap* heap) {
    struct segment *max = NULL;
    int last = 0;
    for (int i = 0; i < heap->size; i++){
        if (i > heap->size - 1) {
            return max == NULL ? NULL : max;
        }
        if ((heap->arr[i]->in_pool == 1 && max == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->erase_counts > max->erase_counts)) {
            max = heap->arr[i];
        }
        if (i == last) {
            if (max != NULL) return max;
            last = right_child(last);
        }
    }
    return NULL;
}

ECMaxHeap* init_ecmaxheap(int capacity) {
    ECMaxHeap* maxheap = (ECMaxHeap*) calloc (1, sizeof(ECMaxHeap));
    maxheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    maxheap->capacity = capacity;
    maxheap->size = 0;
    return maxheap;
}

ECMaxHeap* insert_ecmaxheap(ECMaxHeap* heap, struct segment* element) {
    // Inserts an element to the max heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is larger than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the max heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "ECMaxHeap: Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->ecmax_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts < heap->arr[curr]->erase_counts) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmax_index;
        heap->arr[parent(curr)]->ecmax_index = heap->arr[curr]->ecmax_index;
        heap->arr[curr]->ecmax_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

ECMaxHeap* ecmaxheapify(ECMaxHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the max-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // get the largest node
    int largest = index;

    if (left < heap->size && heap->arr[left]->erase_counts > heap->arr[index]->erase_counts)
        largest = left;

    if (right < heap->size && heap->arr[right]->erase_counts > heap->arr[largest]->erase_counts)
        largest = right;

    // Now if the current element is not the largest,
    // swap with the current element. The max heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (largest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[largest];
        heap->arr[largest] = temp;
        //swap index
        int tmp = heap->arr[index]->ecmax_index;
        heap->arr[index]->ecmax_index = heap->arr[largest]->ecmax_index;
        heap->arr[largest]->ecmax_index = tmp;

        heap = ecmaxheapify(heap, largest);
    }

    return heap;
}

ECMaxHeap* delete_ecmaximum(ECMaxHeap* heap) {
    // Deletes the maximum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->ecmax_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the max-heap
    // property
    heap = ecmaxheapify(heap, 0);
    return heap;
}

ECMaxHeap* ecmax_delete_element(ECMaxHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's larger than the current root
    int cur_erase_counts = heap->arr[index]->erase_counts;
    heap->arr[index]->erase_counts = INT_MAX;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts < heap->arr[curr]->erase_counts) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmax_index;
        heap->arr[parent(curr)]->ecmax_index = heap->arr[curr]->ecmax_index;
        heap->arr[curr]->ecmax_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->erase_counts = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_ecmaximum(heap);
    return heap;
}

ECMaxHeap* ecmax_addone_check(ECMaxHeap* heap, int index) {
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts < heap->arr[curr]->erase_counts) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmax_index;
        heap->arr[parent(curr)]->ecmax_index = heap->arr[curr]->ecmax_index;
        heap->arr[curr]->ecmax_index = tmp;

        curr = parent(curr);
    }
    return heap;
}

void print_ecmaxheap(ECMaxHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("ECMax Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d-> ", heap->arr[i]->erase_counts, heap->arr[i]->ecmax_index);
    }
    printf("\n");
}

void free_ecmaxheap(ECMaxHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EC max heap***/

/***EEC min heap***/
//typedef struct EECMinHeap EECMinHeap;
//struct EECMinHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_eecmin(EECMinHeap* heap) {
    // Return the root node element,
    // since that's the minimum
//    return heap->arr[0];
    struct segment *min = NULL;
    int last = 0;
    for (int i = 0; i < heap->size; i++){
        if (i > heap->size - 1) {
            return min == NULL ? NULL : min;
        }
        if ((heap->arr[i]->in_pool == 1 && min == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->effective_ec < min->effective_ec)) {
            min = heap->arr[i];
        }
        if (i == last) {
            if (min != NULL) return min;
            last = right_child(last);
        }
    }
    return NULL;
}

EECMinHeap* init_eecminheap(int capacity) {
    EECMinHeap* minheap = (EECMinHeap*) calloc (1, sizeof(EECMinHeap));
    minheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    minheap->capacity = capacity;
    minheap->size = 0;
    return minheap;
}

EECMinHeap* insert_eecminheap(EECMinHeap* heap, struct segment* element) {
    // Inserts an element to the min heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is lesser than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the min heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "EECMinHeap: Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->eecmin_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec > heap->arr[curr]->effective_ec) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmin_index;
        heap->arr[parent(curr)]->eecmin_index = heap->arr[curr]->eecmin_index;
        heap->arr[curr]->eecmin_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

EECMinHeap* eecminheapify(EECMinHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the min-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // Variable to get the smallest element of the subtree
    // of an element an index
    int smallest = index;

    // If the left child is smaller than this element, it is
    // the smallest
    if (left < heap->size && heap->arr[left]->effective_ec < heap->arr[index]->effective_ec)
        smallest = left;

    // Similarly for the right, but we are updating the smallest element
    // so that it will definitely give the least element of the subtree
    if (right < heap->size && heap->arr[right]->effective_ec < heap->arr[smallest]->effective_ec)
        smallest = right;

    // Now if the current element is not the smallest,
    // swap with the current element. The min heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (smallest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[smallest];
        heap->arr[smallest] = temp;
        //swap index
        int tmp = heap->arr[index]->eecmin_index;
        heap->arr[index]->eecmin_index = heap->arr[smallest]->eecmin_index;
        heap->arr[smallest]->eecmin_index = tmp;

        heap = eecminheapify(heap, smallest);
    }

    return heap;
}

EECMinHeap* delete_eecminimum(EECMinHeap* heap) {
    // Deletes the minimum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->eecmin_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the min-heap
    // property
    heap = eecminheapify(heap, 0);
    return heap;
}

EECMinHeap* eecmin_delete_element(EECMinHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's lesser than the current root
    int cur_erase_counts = heap->arr[index]->effective_ec;
    heap->arr[index]->effective_ec = -1;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec > heap->arr[curr]->effective_ec) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmin_index;
        heap->arr[parent(curr)]->eecmin_index = heap->arr[curr]->eecmin_index;
        heap->arr[curr]->eecmin_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->effective_ec = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_eecminimum(heap);
    return heap;
}

EECMinHeap* eecmin_addone_check(EECMinHeap* heap, int index) {
    return eecminheapify(heap, index);
}

void print_eecminheap(EECMinHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("EECMin Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d -> ", heap->arr[i]->effective_ec, heap->arr[i]->eecmin_index);
    }
    printf("\n");
}
void free_eecminheap(EECMinHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EC min heap***/

/***EEC max heap***/
//typedef struct EECMaxHeap EECMaxHeap;
//struct EECMaxHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_eecmax(EECMaxHeap* heap) {
    struct segment *max = NULL;
    int last = 0;
    for (int i = 0; i < heap->size; i++){
        if (i > heap->size - 1) {
            return max == NULL ? NULL : max;
        }
        if ((heap->arr[i]->in_pool == 1 && max == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->effective_ec > max->effective_ec)) {
            max = heap->arr[i];
        }
        if (i == last) {
            if (max != NULL) return max;
            last = right_child(last);
        }
    }
    return NULL;
}

EECMaxHeap* init_eecmaxheap(int capacity) {
    EECMaxHeap* maxheap = (EECMaxHeap*) calloc (1, sizeof(EECMaxHeap));
    maxheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    maxheap->capacity = capacity;
    maxheap->size = 0;
    return maxheap;
}

EECMaxHeap* insert_eecmaxheap(EECMaxHeap* heap, struct segment* element) {
    // Inserts an element to the max heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is larger than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the max heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "EECMaxHeap: Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->eecmax_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec < heap->arr[curr]->effective_ec) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmax_index;
        heap->arr[parent(curr)]->eecmax_index = heap->arr[curr]->eecmax_index;
        heap->arr[curr]->eecmax_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

EECMaxHeap* eecmaxheapify(EECMaxHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the max-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // get the largest node
    int largest = index;

    if (left < heap->size && heap->arr[left]->effective_ec > heap->arr[index]->effective_ec)
        largest = left;

    if (right < heap->size && heap->arr[right]->effective_ec > heap->arr[largest]->effective_ec)
        largest = right;

    // Now if the current element is not the largest,
    // swap with the current element. The max heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (largest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[largest];
        heap->arr[largest] = temp;
        //swap index
        int tmp = heap->arr[index]->eecmax_index;
        heap->arr[index]->eecmax_index = heap->arr[largest]->eecmax_index;
        heap->arr[largest]->eecmax_index = tmp;

        heap = eecmaxheapify(heap, largest);
    }

    return heap;
}

EECMaxHeap* delete_eecmaximum(EECMaxHeap* heap) {
    // Deletes the maximum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->eecmax_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the max-heap
    // property
    heap = eecmaxheapify(heap, 0);
    return heap;
}

EECMaxHeap* eecmax_delete_element(EECMaxHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's larger than the current root
    int cur_erase_counts = heap->arr[index]->effective_ec;
    heap->arr[index]->effective_ec = 99999;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec < heap->arr[curr]->effective_ec) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmax_index;
        heap->arr[parent(curr)]->eecmax_index = heap->arr[curr]->eecmax_index;
        heap->arr[curr]->eecmax_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->effective_ec = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_eecmaximum(heap);
    return heap;
}

EECMaxHeap* eecmax_addone_check(EECMaxHeap* heap, int index) {
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec < heap->arr[curr]->effective_ec) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmax_index;
        heap->arr[parent(curr)]->eecmax_index = heap->arr[curr]->eecmax_index;
        heap->arr[curr]->eecmax_index = tmp;

        curr = parent(curr);
    }
    return heap;
}

void print_eecmaxheap(EECMaxHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("EECMax Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d-> ", heap->arr[i]->effective_ec, heap->arr[i]->eecmax_index);
    }
    printf("\n");
}

void free_eecmaxheap(EECMaxHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EEC max heap***/
