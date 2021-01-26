#
# file:        2hc.py
# description: simulation of 2-part non-uniform traffic mixes
#
# Testing wear leveling module


import getaddr
import ftlsim
import random
import sys

sys.stdout = open('wl_res.txt', 'a', 1)

# 256k * 64 * 4K = 64GB

U = 10000
Np = 64
minfree = 4

def cumulative_sum(x):
    return reduce(lambda a,b: a + [a[-1]+b], x, [0])
    
warmup = getaddr.uniform(U*Np)

steps = [ i * 0.05 for i in range(1,20)]
steps_r = [0.5, 0.8, 0.9]
steps_f = [0.5, 0.2, 0.05]

for wl_threshold in [10]:
    for S_f in [0.1, 0.5, 0.8]:
        alpha = 1 / (1-S_f)
        T = int(U * alpha) + Np
        
        # hot fraction (of LBA space) = f, hot rate (of writes) = r
        # iterate over r,f = (0.05...0.95) where r >= f
        #
        for f in steps_f:
            maxes = [int(U*Np*f), int(U*Np*(1-f))]
            bases = cumulative_sum(maxes)[0:-1]

            for r in steps_r:
                if (r + f != 1.0):
                    continue

                # first we create the data source
                #
                tops = [r, 1]
                addr = getaddr.mixed()
                for b,m,t in zip(bases, maxes, tops):
    #                print "b", b, "m", m, "t", t
                    aa = getaddr.uniform(m)
                    aa.thisown = 0
                    addr.add(aa.handle, t, b)

                # then the simulator and flash blocks
                #
                ftl = ftlsim.ftl(T, Np)
                ftl.minfree = minfree
                ftl.wl_threshold = wl_threshold
                ftl.wl_activated = 0
                pool = ftlsim.pool(ftl, "greedy", Np)
                pool.next_pool = pool

                freelist = [ftlsim.segment(Np) for i in range(T+minfree)]
                for b in freelist:
                    b.thisown = False
                    b.type = random.randint(0, 1) # Initially a block arbitrarily joins one of the two pools.
                pool.add_segment(freelist.pop())
                for b in freelist:
                    ftl.put_blk(b)

                # use a sequential address source to write each page once
                #
                seq = getaddr.seq()
                ftl.run(seq.handle, U*Np)

                # and now warm up the simulation
                #
                ftl.run(warmup.handle, T*Np*2)

                ftl.int_writes = 0
                ftl.ext_writes = 0

                # finally run for a total of 5 full over-writes, printing
                # stats every 1/10th capacity
                #
#                for i in range(1000):
#                    ftl.run(warmup.handle, T*Np)
                print "wl_threshold %d" % wl_threshold
                print "S_f %.3f" % S_f
                print "r/f %.2f %.2f" % (r, f)
                ftl.wl_activated = 1
                for i in range(10):
                    ftl.int_writes = 0
                    ftl.ext_writes = 0
                    ftl.wl_counts  = 0
                    ftl.wl_writes  = 0
                    ftl.wl_cpr = 0
                    ftl.wl_hpr = 0
                    ftl.run(addr.handle, U*Np)
                    print "%d %d %f %d %f %f" % (ftl.int_writes, ftl.ext_writes,
                                        1.0 * ftl.int_writes / (1 + ftl.ext_writes), ftl.wl_counts, 1.0 * ftl.wl_writes / (1 + ftl.ext_writes), (1.0 * ftl.int_writes + ftl.wl_writes) / (1 + ftl.ext_writes))
                                        
#                    hot_count = 0
#                    for b in freelist:
#                        if b.type == 1:
#                            hot_count += 1
##                        print "type = %d erase_count = %d" % (b.type, b.erase_counts)
#                    print "hot_count = %d cpr = %d hpr = %d" % (hot_count, ftl.wl_cpr, ftl.wl_hpr)
            

