#
# file:        2hc.py
# description: simulation of 2-part non-uniform traffic mixes
#
# Testing wear leveling module


import getaddr
import ftlsim
import random
import sys
import datetime

filename = "pwl_skew_smallprint_75"
sys.stdout = open(filename + '.txt', 'a+', 1)

# 256k * 64 * 4K = 64GB

U = 256 * 1024
Np = 256
minfree = 7

footprint = 0.05
steps_r = [0.9]
steps_f = [0.1]

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

for wl_threshold in [5]:
    for S_f in [0.1]:
        alpha = 1 / (1 - S_f)
        T = int(U * alpha)
        
        # hot fraction (of LBA space) = f, hot rate (of writes) = r
        # iterate over r,f = (0.05...0.95) where r >= f
        #
        for f in steps_f:
            maxes = [int(U * Np * f * footprint), int(footprint * U * Np * (1 - f))]
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
                ftl.U = U
                ftl.endurance = 500
                ftl.minfree = minfree
                ftl.wl_threshold = wl_threshold
                ftl.wl_threshold_ap = 8
                ftl.wl_activated = 0
                pool = ftlsim.pool(ftl, "greedy", Np)
                pool.next_pool = pool
                print "Total number of blocks: %d" % (T + minfree)
                freelist = [ftlsim.segment(Np) for i in range(T + minfree)]
                id = 0
                for b in freelist:
                    b.thisown = False
                    # b.erase_counts = random.randint(0, 50)
                    # b.read_counts = random.randint(9990, 10000)
                    b.type = random.randint(0, 1)  # Initially a block arbitrarily joins one of the two pools.
                    b.id = id
                    id = id + 1
                pool.add_segment(freelist.pop())
                for b in freelist:
                    ftl.put_blk(b)
                
                # use a sequential address source to write each page once
                #
                seq = getaddr.seq()
                ftl.run(seq.handle, U * Np)
                
                # and now warm up the simulation
                #
                ftl.run(warmup.handle, 3 * U * Np)
                
                ftl.int_writes = 0
                ftl.ext_writes = 0
                ftl.wl_activated = 1
                ftl.wl_ds = 1
                print "Loading heap module..."
                ftl.build_heap()
              
                print "pwl r=%.2f f=%.2f" % (r, f)
                print "pwl footprint %.2f" % footprint
                print "S_f %.3f" % S_f
                print "Np %d" % Np
                print "U %d" % U
                print "T %d" % T
                print "Endurance %d" % ftl.endurance
                
                res = 0
                request = 0
                while True:
                    addr = getLba(r, f, int(U*footprint), Np)
                    res = ftl.write(addr)
                    request = request + 1
                    if request == (U * Np):
                        print "%d %d %f %d %f %f %d %d" % (ftl.int_writes, ftl.ext_writes,
                                                           1.0 * ftl.int_writes / (1 + ftl.ext_writes),
                                                           ftl.wl_counts,
                                                           1.0 * ftl.wl_writes / (1 + ftl.ext_writes),
                                                           (1.0 * ftl.int_writes + ftl.wl_writes) / (
                                                                       1 + ftl.ext_writes),
                                                           ftl.hpr, ftl.cpr)
                        request = 0
                        ftl.int_writes = 0
                        ftl.ext_writes = 0
                        ftl.wl_counts = 0
                        ftl.wl_writes = 0
                        ftl.hpr = 0
                        ftl.cpr = 0
                        ftl.TBW = ftl.TBW + 1
                    if ftl.TBW == 100:
                        print "%d %d %f %d %f %f %d %d" % (ftl.int_writes, ftl.ext_writes,
                                                           1.0 * ftl.int_writes / (1 + ftl.ext_writes),
                                                           ftl.wl_counts,
                                                           1.0 * ftl.wl_writes / (1 + ftl.ext_writes),
                                                           (1.0 * ftl.int_writes + ftl.wl_writes) / (
                                                                   1 + ftl.ext_writes),
                                                           ftl.hpr, ftl.cpr)
                        print " "
                        print "TBW(U*Np) %f" % (1.0*ftl.TBW + (1.0*ftl.ext_writes/(U * Np)))
                        print "bad %d" % ftl.bad_blocks
                        currentDT = datetime.datetime.now()
                        print (str(currentDT))
                        print " "
                        sys.stdout = open(filename + '_wear.txt', 'a+', 1)
                        for b in freelist:
                            print "%d %d" % (b.erase_counts, b.type)
                        break
                

