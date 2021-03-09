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
import ftlsim

# parameters
#
U = 23020
Np = 128
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

# LRU-managed pool
#
lru = ftlsim.pool(ftl, "lru", Np)
# lru.next_pool = lru

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
src = getaddr.uniform(U*Np)
ftl.run(src.handle, U*Np)
# print src.handle.next()
# print src.handle.next()
# print src.handle.next()
# print src.handle.next()
print ("ready...")

#duplicate ftl
# ftll = ftlsim.ftl(T, Np)
# ftll.minfree = minfree
# ftll.get_input_pool = ftlsim.cvar.write_select_first
# ftll.get_pool_to_clean = ftlsim.cvar.clean_select_first
# lrul = ftlsim.pool(ftll, "lru", Np)
# lrul.next_pool = lrul
# freelistl = [ftlsim.segment(Np) for i in range(T)]
# for b in freelistl:
#     b.thisown = False
# lrul.add_segment(freelistl.pop())
# for b in freelistl:
#     ftll.put_blk(b)
# srcl = getaddr.uniform(U*Np)
# ftll.run(srcl.handle, U*Np)
# Now run with uniform random traffic for 10 units of 0.1 volume each.
#
for i in range(100):
    ftl.ext_writes = 0
    ftl.int_writes = 0
    seq = getaddr.seq()
    # ftll.run(seq.handle, U*Np/2)
    ftl.run(seq.handle, U*Np)
    print ftl.ext_writes, ftl.int_writes, (1.0*ftl.int_writes)/ftl.ext_writes
    # ftl.run(src.handle, U * Np)
    

