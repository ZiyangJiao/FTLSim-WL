#
# file:        2hc.py
# description: simulation of 2-part non-uniform traffic mixes
#
# Testing wear leveling module


import getaddr
import ftlsim
import random
import sys

# sys.stdout = open('wl_res.txt', 'a', 1)

# 256k * 64 * 4K = 64GB

U = 256 * 1024
Np = 256
minfree = 4


def cumulative_sum(x):
    return reduce(lambda a, b: a + [a[-1] + b], x, [0])

def getLba(r, f, U, Np):
    a = random.randint(0, int(f * U * Np - 1))
    b = random.randint(int(f * U * Np), int(U * Np - 1))
    if random.random() < r:
        return a
    else:
        return b
    
warmup = getaddr.uniform(U * Np)

steps = [i * 0.05 for i in range(1, 20)]
steps_r = [0.9]
steps_f = [0.1]

for wl_threshold in [50, 100, 200]:
    for S_f in [0.1, 0.07]:
        alpha = 1 / (1 - S_f)
        T = int(U * alpha) + Np
        
        # hot fraction (of LBA space) = f, hot rate (of writes) = r
        # iterate over r,f = (0.05...0.95) where r >= f
        #
        for f in steps_f:
            maxes = [int(U * Np * f), int(U * Np * (1 - f))]
            bases = cumulative_sum(maxes)[0:-1]
            
            for r in steps_r:
                if (r + f != 1.0):
                    continue
                
                # first we create the data source
                #
                tops = [r, 1]
                addr = getaddr.mixed()
                for b, m, t in zip(bases, maxes, tops):
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
                
                freelist = [ftlsim.segment(Np) for i in range(T + minfree)]
                for b in freelist:
                    b.thisown = False
                    # b.erase_counts = random.randint(0, 50)
                    b.read_counts = random.randint(9990, 10000)
                    b.type = random.randint(0, 1)  # Initially a block arbitrarily joins one of the two pools.
                pool.add_segment(freelist.pop())
                for b in freelist:
                    ftl.put_blk(b)
                
                # use a sequential address source to write each page once
                #
                seq = getaddr.seq()
                ftl.run(seq.handle, U * Np)
                
                # and now warm up the simulation
                #
                ftl.run(warmup.handle, T * Np)
                
                ftl.int_writes = 0
                ftl.ext_writes = 0
                
              
                print "wl_threshold %d" % wl_threshold
                print "S_f %.3f" % S_f
                print "r/f %.2f %.2f" % (r, f)
                print "Np %d" % Np
                print "U %d" % U
                
                ftl.wl_activated = 1
                ftl.rr_threshold = 10000
                for i in range(100):
                    ftl.int_writes = 0
                    ftl.ext_writes = 0
                    ftl.wl_counts = 0
                    ftl.wl_writes = 0
                    # ftl.run(addr.handle, U * Np)
                    for j in range(U*Np):
                        ftl.write(getLba(0.9, 0.1, U, Np))
                    # max = 0
                    # min = 99999999
                    # for b in freelist:
                    #     if b.type == 1 and b.erase_counts > max:
                    #         max = b.erase_counts
                    #     elif b.type == 0 and b.erase_counts < min:
                    #         min = b.erase_counts
                    # print "max %d min %d" % (max, min)
                    print "%d %d %f %d %f %f" % (ftl.int_writes, ftl.ext_writes,
                                                 1.0 * ftl.int_writes / (1 + ftl.ext_writes), ftl.wl_counts,
                                                 1.0 * ftl.wl_writes / (1 + ftl.ext_writes),
                                                 (1.0 * ftl.int_writes + ftl.wl_writes) / (1 + ftl.ext_writes))
                

                # for a in range(1000 * U * Np):
                #     # ftl.write(getLba(0.5, 0.5, U, Np))
                #     ftl.read(getLba(0.5, 0.5, U, Np))
                #     i += 1
                #     if i >= U * Np:
                #         i = 0
                #         # print ftl.ext_writes, ftl.int_writes, (1.0 * ftl.int_writes) / ftl.ext_writes
                #         print ftl.ext_reads, (1.0 * ftl.rr_writes) / (ftl.ext_reads + 1), ftl.rr_counts, ftl.rr_writes
                #         ftl.ext_writes = 0
                #         ftl.int_writes = 0
                #         ftl.rr_counts = 0
                #         ftl.rr_writes = 0
                #         ftl.ext_reads = 0

