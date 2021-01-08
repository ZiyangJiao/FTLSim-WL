#
# file:        two-pool.py
# description: Greedy cleaning with hot and cold separation
#
# Peter Desnoyers, Northeastern University, 2012
#
# Copyright 2012 Peter Desnoyers
# This file is part of ftlsim.
# ftlsim is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version. 
#
# ftlsim is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details. 
# You should have received a copy of the GNU General Public License
# along with ftlsim. If not, see http://www.gnu.org/licenses/. 
#

import getaddr
import ftlsim

# FTL parameters
#
U = 23020
Np = 128
S_f = 0.07
alpha = 1 / (1-S_f)
minfree = Np / 2
T = int(U * alpha) 

# traffic parameters - 80% of traffic to 20% of LBA space
#
r = 0.8
f = 0.2

U_h = int(f*U*Np)
U_c = U*Np - U_h

# create the traffic source
#
src_h = getaddr.uniform(U_h)
src_c = getaddr.uniform(U_c)
mix = getaddr.mixed()
for src,frac,base in ((src_h,r,0), (src_c,1.0,U_h)):
    src.thisown = False
    mix.add(src.handle, frac, base)

# and now the hot/cold oracle - we know that if LBA < U_h then it's a
# hot page.
#
def is_hot(a):
    global U_h
    return a < U_h

# create an FTL with a hot pool and a cold pool. Use python select
# scripts to route hot LBAs to hot pool, and to do global greedy
# cleaning.
#
ftl = ftlsim.ftl(U, Np)
ftl.minfree = minfree
ftl.get_input_pool = ftlsim.cvar.write_select_python
ftl.get_pool_to_clean = ftlsim.cvar.clean_select_python

hot_pool = ftlsim.pool(ftl, "greedy", Np)
cold_pool = ftlsim.pool(ftl, "greedy", Np)

# perfect knowledge of hot/cold data
#
def perfect_hc(lba):
    global hot_pool, cold_pool
    if is_hot(lba):
        ftlsim.return_pool(hot_pool)
    else:
        ftlsim.return_pool(cold_pool)
ftl.get_input_pool_arg = perfect_hc

# global greedy cleaning - select the pool to clean based on the
# utilization of the next segment to be cleaned.
#
def global_greedy():
    global hot_pool, cold_pool
    if hot_pool.tail_util() < cold_pool.tail_util():
        ftlsim.return_pool(hot_pool)    # kludge, since we can't return
    else:                               # a wrapped object
        ftlsim.return_pool(cold_pool)
ftl.get_pool_to_clean_arg = global_greedy

# Allocate segments...
#
freelist = [ftlsim.segment(Np) for i in range(T+minfree)]
for b in freelist:
    b.thisown = False

hot_pool.add_segment(freelist.pop())
cold_pool.add_segment(freelist.pop())
for b in freelist:
    ftl.put_blk(b)

# use a sequential address source to write each page once
#
seq = getaddr.seq()
ftl.run(seq.handle, U*Np);

# warm up with 2 volumes worth of uniform traffic
#
src_u = getaddr.uniform(U*Np)
ftl.run(src_u.handle, 2*U*Np);

# Finally run with mixed traffic source for 30 steps of 0.1 volume
# each.  
#
sum_e,sum_i = 0,0

for i in range(30):
    ftl.ext_writes = 0
    ftl.int_writes = 0
    ftl.run(src.handle, U*Np/10)
    print ftl.ext_writes, ftl.int_writes, (1.0*ftl.int_writes)/ftl.ext_writes
    sum_e += ftl.ext_writes
    sum_i += ftl.int_writes

print (1.0*sum_i)/sum_e

