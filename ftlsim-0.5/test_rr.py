#
# file:        lru-high.py
# description: Example LRU cleaning simulation using high-speed routines
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
import genaddr
import ftlsim
import random

def getLba(r, f, U, Np):
    a = random.randint(0, f * U * Np - 1)
    b = random.randint(f * U * Np, U * Np - 1)
    if random.random() < r:
        return a
    else:
        return b

# parameters
#
U = 10000
Np = 64
S_f = 0.07
alpha = 1 / (1-S_f)
minfree = Np
T = int(U * alpha) + minfree

# FTL with default parameters for single pool
#
ftl = ftlsim.ftl(T, Np)
ftl.minfree = minfree
ftl.get_input_pool = ftlsim.cvar.write_select_first
ftl.get_pool_to_clean = ftlsim.cvar.clean_select_first
# ftl.wl_threshold = 1000
ftl.rr_threshold = 128


# LRU-managed pool
#
lru = ftlsim.pool(ftl, "greedy", Np)
lru.next_pool = lru

# allocate blocks and put them on FTL free list, except for
# 1 write frontier.
#
freelist = [ftlsim.segment(Np) for i in range(T)]
for b in freelist:
    b.thisown = False
lru.add_segment(freelist.pop())
for b in freelist:
    ftl.put_blk(b)

# use a sequential address source to write each page once
#
seq = getaddr.seq()
ftl.run(seq.handle, U*Np)
i,j = 0,0
# warm up the simulation
#
src = genaddr.uniform(U*Np)
for a in src.addrs():
    ftl.write(getLba(0.8, 0.2, U, Np))
    i += 1
    if i >= 2 * U * Np:
        i = 0
        break


ftl.ext_writes = 0
ftl.int_writes = 0
ftl.wl_counts  = 0

print ("ready...")

# Now run with uniform random traffic for 10 units of 0.1 volume each.
#
# for a in src.addrs():
for a in range(100*U*Np):
    # ftl.write(getLba(0.5, 0.5, U, Np))
    ftl.read(getLba(0.9, 0.1, U, Np))
    i += 1
    if i >= U * Np:
        i = 0
        # print ftl.ext_writes, ftl.int_writes, (1.0 * ftl.int_writes) / ftl.ext_writes
        print ftl.ext_reads, (1.0 * ftl.rr_writes) / (ftl.ext_reads + 1), ftl.rr_counts, ftl.rr_writes
        ftl.ext_writes = 0
        ftl.int_writes = 0
        ftl.rr_counts  = 0
        ftl.rr_writes  = 0
        ftl.ext_reads  = 0
