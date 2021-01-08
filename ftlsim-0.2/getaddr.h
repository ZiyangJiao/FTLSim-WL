/*
 * file:        getaddr.h
 * see getaddr.i for details
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

struct seq {
    struct getaddr handle;
    int next;
};
struct seq *seq_new(void);

struct uniform {
    struct getaddr handle;
    int max;
};

struct uniform *uniform_new(int max);

struct mixed {
    struct getaddr handle;
    struct mixed_private *private_data;
};

struct mixed *mixed_new(void);
void mixed_do_add(struct mixed *self, struct getaddr *g, double p, int base);
void mixed_del(struct mixed *self);

struct trace {
    struct getaddr handle;
    int eof, single;
    struct trace_private *private_data;
};

struct trace *trace_new(char *file);
void trace_del(struct trace *t);

struct log {
    struct getaddr handle;
    struct log_private *private_data;
};

struct log *log_new(struct getaddr *src, char *file);
void log_close(struct log *l);

struct scramble {
    struct getaddr handle;
    int eof;
    struct scramble_private *private_data;
};

struct scramble *scramble_new(struct getaddr *src, int max);
void scramble_del(struct scramble *self);

int next(struct getaddr *);
