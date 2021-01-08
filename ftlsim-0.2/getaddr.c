/*
 * file:        getaddr.c
 * * description: Address generation for fast FTL simulation.
 *
 * External C code uses the 'struct getaddr' handle to interface
 * directly - h->getaddr(h->private_data) returns the next address. 
 *
 * Peter Desanoyers, Northeastern University, 2012
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
#include <assert.h>

#include "ftlsim.h"
#include "getaddr.h"

int seq_get(void *private_data)
{
    struct seq *seq = private_data;
    return seq->next++;
}

struct seq *seq_new(void)
{
    struct seq *seq = calloc(sizeof(*seq), 1);
    seq->handle.getaddr = seq_get;
    seq->handle.del = (void*)free;
    return seq;
}

int next(struct getaddr *g)
{
    return g->getaddr(g->private_data);
}

int uniform_get(void *private_data)
{
    struct uniform *u = private_data;
    double r1 = (double)rand() / RAND_MAX;
    return r1 * u->max;
}
    
struct uniform *uniform_new(int max)
{
    struct uniform *u = calloc(sizeof(*u), 1);
    u->max = max;
    u->handle.private_data = u;
    u->handle.getaddr = uniform_get;
    u->handle.del = (void*)free;
    return u;
}

struct mixed_private {
    int n;
    double p[5];
    int base[5];
    struct getaddr *gen[5];
};

int mixed_get(void *private_data)
{
    struct mixed *m = private_data;
    struct mixed_private *p = m->private_data;
    int i;
    double r = (double)rand() / RAND_MAX;
    for (i = 0; i < p->n; i++)
        if (r < p->p[i])
            return p->base[i] + next(p->gen[i]);
    return 0;
}

void mixed_del(struct mixed *self)
{
    int i;
    struct mixed_private *p = self->private_data;
    for (i = 0; i < p->n; i++)
        if (p->gen[i]->del)
            p->gen[i]->del(p->gen[i]->private_data);
    free(p);
    free(self);
}

struct mixed *mixed_new(void)
{
    struct mixed *m = calloc(sizeof(*m), 1);
    m->handle.private_data = m;
    m->handle.getaddr = mixed_get;
    m->private_data = calloc(sizeof(*m->private_data), 1);
    return m;
}

void mixed_do_add(struct mixed *self, struct getaddr *g, double p, int base)
{
    struct mixed_private *priv = self->private_data;
    int i = priv->n;            /* ignore overflow */
    priv->gen[i] = g;
    priv->p[i] = p;
    priv->base[i] = base;
    priv->n++;
}

/*
 * getaddr_trace(filename) - reads lines containing addr,len pairs.
 *  addresses and lengths beginning with '0x' will be parsed as hex
 *  addresses are in units of *PAGES*, not sectors or bytes
 */
struct trace_private {
    FILE *fp;
    int addr, count;
};

static int trace_get(void *private_data)
{
    struct trace *t = private_data;
    struct trace_private *p = t->private_data;

    if (p->count > 0) {
        p->count--;
        return ++(p->addr);
    }

    char line[80], *tmp;
    if (fgets(line, sizeof(line), p->fp) == NULL) {
        t->eof = 1;
        return -1;
    }
    if (t->single)
        return atoi(line);
    
    p->addr = strtol(line, &tmp, 0);
    p->count = strtol(tmp, NULL, 0) - 1;
    return p->addr;
}

void trace_del(struct trace *t)
{
    fclose(t->private_data->fp);
    free(t->private_data);
    free(t);
}

struct trace *trace_new(char *file)
{
    struct trace *t = calloc(sizeof(*t), 1);
    t->handle.private_data = t;
    t->handle.getaddr = trace_get;
    t->handle.del = (void*)trace_del;
    
    struct trace_private *p = calloc(sizeof(*p), 1);
    p->fp = fopen(file, "r");
    t->private_data = p;
    
    return t;
}

struct log_private {
    struct getaddr *src;
    FILE *fp;
};

int log_get(void *private_data)
{
    struct log *l = private_data;
    int a = next(l->private_data->src);
    fprintf(l->private_data->fp, "%d\n", a);
    return a;
}

void log_close(struct log *l)
{
    struct log_private *p = l->private_data;
    fclose(p->fp);
    if (p->src->del)
        p->src->del(p->src->private_data);
    free(l->private_data);
    free(l);
}
   
struct log *log_new(struct getaddr *src, char *file)
{
    struct log_private *priv = calloc(sizeof(*priv), 1);
    priv->src = src;
    priv->fp = fopen(file, "w");

    struct log *val = calloc(sizeof(*val), 1);
    val->handle.private_data = val;
    val->handle.getaddr = log_get;
    val->handle.del = (void*)log_close;
    val->private_data = priv;

    return val;
}

struct scramble_private {
    int max;
    int *permutation;
    struct getaddr *src;
};

void scramble_del(struct scramble *self)
{
    struct scramble_private *p = self->private_data;
    if (p->src->del)
        p->src->del(p->src->private_data);
    free(p);
    free(self);
}

int scramble_get(void *private_data)
{
    struct scramble *s = private_data;
    struct scramble_private *p = s->private_data;
    int a = next(p->src);
    if (a == -1) {
        s->eof = 1;
        return -1;
    }
    return p->permutation[a];
}

struct scramble *scramble_new(struct getaddr *src, int max)
{
    int i;
    struct scramble_private *priv = calloc(sizeof(*priv), 1);
    priv->src = src;
    priv->max = max;
    priv->permutation = malloc(sizeof(int)*max);

    for (i = 0; i < max; i++)   /* Knuth Shuffle */
        priv->permutation[i] = i;
    for (i = 0; i < max; i++) {
        int tmp, x = 1.0 * rand() * (max-i) / RAND_MAX;
        assert(x+i < max);
        tmp = priv->permutation[x+i];
        priv->permutation[x+i] = priv->permutation[i];
        priv->permutation[i] = tmp;
    }
        
    struct scramble *val = calloc(sizeof(*val), 1);
    val->handle.private_data = val;
    val->handle.getaddr = scramble_get;
    val->handle.del = (void*)scramble_del;
    val->private_data = priv;

    return val;
}
    
