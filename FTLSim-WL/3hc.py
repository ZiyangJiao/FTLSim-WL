#
# file:        3hc.py
# description: simulation of 3-part non-uniform traffic mixes
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

splits = ((1/2.0, 1/4.0, 1/4.0),
          (2/3.0, 2/9.0, 1/9.0),
          (3/4.0, 3/16.0, 1/16.0))

def cumulative_sum(x):
    return reduce(lambda a,b: a + [a[-1]+b], x, [0])[0:-1]
    
warmup = getaddr.uniform(U*Np)

for S_f in (0.07, 0.10, 0.13):
    alpha = 1 / (1-S_f)
    T = int(U * alpha) + Np

    # iterate across splits in traffic rates
    for s in splits:
        maxes = [int(U * np * f) for f in s]
        bases = cumulative_sum(maxes)
        
        # and across LBA fraction for each split
        #
        for a in splits:
            rates = a[-1::-1]
            tops = cumulative_sum(rates)[1:] + [1]

            # first we create the data source
            #
            addr = getaddr.mixed()
            for b,m,t in zip(bases, maxes, tops):
                aa = getaddr.uniform(m)
                aa.thisown = 0
                addr.add(aa.handle, t, b)

            # then the simulator and flash blocks
            #
            ftl = ftlsim.lru(T, Np);
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

            print "S_f %.3f" % S_f
            print "f   %.2f %.2f %.2f" % s
            print "r   %.2f %.2f %.2f" % rates
            for i in range(50):
                ftl.run(addr.handle, U*Np/10)
                print "%d %d %f" % (ftl.int_writes, ftl.ext_writes,
                                    1.0 * ftl.int_writes / (1 + ftl.ext_writes))
                ftl.int_writes = 0
                ftl.ext_writes = 0
