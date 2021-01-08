#
# file:        windowed-greedy.py
# description: Example Windowed Greedy simulation. See:
#              "Write amplification analysis in flash-based solid state
#              drives" Xiao-Yu Hu, Evangelos Eleftherius, Robert Haas,
#              Ilias Iliadis, and Roman Pletka, SYSTOR 2009.
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

# parameters
#
U = 23020
Np = 128
S_f = 0.07
alpha = 1 / (1-S_f)
minfree = Np
T = int(U * alpha) + minfree
greedy_len = 500

# FTL with default parameters for single pool
#
ftl = ftlsim.ftl(T, Np)
ftl.minfree = minfree
ftl.get_input_pool = ftlsim.cvar.write_select_first
ftl.get_pool_to_clean = ftlsim.cvar.clean_select_python

# LRU pool
lru = ftlsim.pool(ftl, "lru", Np)

# Greedy-managed pool
#
gdy = ftlsim.pool(ftl, "greedy", Np)
gdy.next_pool = lru

# Cleaning - move blocks from the tail of the LRU queue into the
# greedy pool until we have 'greedy_len' blocks in the greedy window,
# and then let ftlsim choose from the greedy pool.
#
def clean_select():
    global lru, gdy
    #print lru.length, gdy.length, lru.pages_invalid, gdy.pages_invalid
    while gdy.length < greedy_len:
        b = lru.remove_segment()
        gdy.insert_segment(b)
    if gdy.pages_invalid > 1:
        ftlsim.return_pool(gdy)
    else:
        ftlsim.return_pool(lru)

ftl.get_pool_to_clean_arg = clean_select

# Allocate segments...
#
freelist = [ftlsim.segment(Np) for i in range(T)]
for b in freelist:
    b.thisown = False
lru.add_segment(freelist.pop())
gdy.add_segment(freelist.pop())
for b in freelist:
    ftl.put_blk(b)

# use a sequential address source to write each page once
#
seq = getaddr.seq()
ftl.run(seq.handle, U*Np);

# Now run with uniform random traffic for 30 units of 0.1 volume each.
#
sum_e = 0
sum_i = 0
src = getaddr.uniform(U*Np)
for i in range(30):
    ftl.ext_writes = 0
    ftl.int_writes = 0
    ftl.run(src.handle, U*Np/10)
    print ftl.ext_writes, ftl.int_writes, (1.0*ftl.int_writes)/ftl.ext_writes
    sum_e += ftl.ext_writes
    sum_i += ftl.int_writes

print (1.0*sum_i)/sum_e
