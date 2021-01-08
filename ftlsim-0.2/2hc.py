#
# file:        2hc.py
# description: simulation of 2-part non-uniform traffic mixes
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

# 256k * 64 * 4K = 64GB

U = 256*1024
Np = 64
minfree = 4

def cumulative_sum(x):
    return reduce(lambda a,b: a + [a[-1]+b], x, [0])
    
warmup = getaddr.uniform(U*Np)

steps = [ i * 0.05 for i in range(1,20)]

for S_f in (0.07, 0.10, 0.13):
    alpha = 1 / (1-S_f)
    T = int(U * alpha) + Np

    # hot fraction (of LBA space) = f, hot rate (of writes) = r
    # iterate over r,f = (0.05...0.95) where r >= f
    #
    for f in steps:
        maxes = [int(U*Np*f), int(U*Np*(1-f))]
        bases = cumulative_sum(maxes)[0:-1]

        for r in steps:
            if r < f:
                continue

            # first we create the data source
            #
            tops = [r, 1]
            addr = getaddr.mixed()
            for b,m,t in zip(bases, maxes, tops):
                print "b", b, "m", m, "t", t
                aa = getaddr.uniform(m)
                aa.thisown = 0
                addr.add(aa.handle, t, b)

            # then the simulator and flash blocks
            #
            ftl = ftlsim.ftl(T, Np)
            ftl.minfree = minfree
            pool = ftlsim.pool(ftl, "lru", Np)

            freelist = [ftlsim.segment(Np) for i in range(T+minfree)]
            for b in freelist:
                b.thisown = False
            pool.add_segment(freelist.pop())
            for b in freelist:
                ftl.put_blk(b)

            # use a sequential address source to write each page once
            #
            seq = getaddr.seq()
            ftl.run(seq.handle, U*Np);
            
            # and now warm up the simulation
            #
            ftl.run(warmup.handle, T*Np*2)
            
            ftl.int_writes = 0
            ftl.ext_writes = 0

            # finally run for a total of 5 full over-writes, printing
            # stats every 1/10th capacity
            #
            print "S_f %.3f" % S_f
            print "r/f %.2f %.2f" % (r, f)
            for i in range(50):
                ftl.run(addr.handle, U*Np/10)
                print "%d %d %f" % (ftl.int_writes, ftl.ext_writes,
                                    1.0 * ftl.int_writes / (1 + ftl.ext_writes))
                ftl.int_writes = 0
                ftl.ext_writes = 0
