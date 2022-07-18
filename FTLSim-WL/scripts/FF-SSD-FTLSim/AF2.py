#
# file:        2hc.py
# description: simulation of 2-part non-uniform traffic mixes
#
# Testing wear leveling module


import getaddr
import ftlsim
import random
import sys
import os
import numpy as np
import pandas as pd
from sklearn.linear_model import LinearRegression

filename = "../256GB_DisplayAdsDataServer.trace"
outfile = "PWL_AF2_DisplayAdsDataServer.wear"

# 256k * 256 * 4K = 256GB

U = 256 * 1024
Np = 256
minfree = 7


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

for wl_threshold in [4]:
    for S_f in [0.1]:
        alpha = 1 / (1 - S_f)
        T = int(U * alpha)

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
                ftl.U = U
                ftl.endurance = 500
                ftl.minfree = minfree
                ftl.wl_threshold = wl_threshold
                ftl.wl_threshold_ap = 8
                ftl.wl_activated = 0
                pool = ftlsim.pool(ftl, "greedy", Np)
                pool.next_pool = pool
                freelist = [ftlsim.segment(Np) for i in range(T + minfree)]
                print "Total number of blocks: %d" % (T + minfree)

                id = 0
                for b in freelist:
                    b.thisown = False
                    b.type = random.randint(0, 1)  # Initially a block arbitrarily joins one of the two pools.
                    b.id = id
                    id = id + 1
                pool.add_segment(freelist[0])
                for b in freelist[1:]:
                    ftl.put_blk(b)

                # use a sequential address source to write each page once
                #
                seq = getaddr.seq()
                ftl.run(seq.handle, U * Np)

                # and now warm up the simulation
                #
                ftl.run(warmup.handle, U * Np * 2)

                ftl.int_writes = 0
                ftl.ext_writes = 0
                ftl.wl_activated = 1
                ftl.wl_ds = 1
                ftl.TBW = 0
                # print "Loading heap module..."
                ftl.build_heap()


                # ftl.run(addr.handle, 100)
                request = 0
                res = 0
                last = []
                prediction = []
                infile = open(filename, 'r')
                runs = 100
                while True:
                    if res == 1:
                        break
                    infile.seek(0)
                    for line in infile:
                        args = line.split(' ')
                        addr = int(args[1])
                        res = ftl.write(addr)
                        request = request + 1                        

                    	if res == 1:
                            file = open(outfile, "a")
                            last = [0 for i in range(len(freelist))]
                            for b in freelist:
                                last[b.id] = b.erase_counts
                            for ec in last:
                                file.write(str(ec) + " ")
                            file.write("\n")
                            file.close()
                            exit()
                        
                    request = 0
                    runs = runs - 1
                    ftl.int_writes = 0
                    ftl.ext_writes = 0
                    ftl.wl_counts = 0
                    ftl.wl_writes = 0
                    ftl.hpr = 0
                    ftl.cpr = 0
                    ftl.TBW = ftl.TBW + 1

                    file = open(outfile, "a")
                    last = [0 for i in range(len(freelist))]
                    for b in freelist:
                        last[b.id] = b.erase_counts
                    for ec in last:
                        file.write(str(ec) + " ")
                    file.write("\n")
                    file.close()
                                                                               
                    if runs == 0:
                    	exit()

                    if ftl.TBW == 1:
                        prediction = [0 for i in range(len(freelist))]
                        for b in freelist:
                            prediction[b.id] = b.erase_counts
                    if ftl.TBW >= 2: # %save half simulation time
                        for b in freelist:
                            prediction[b.id] = min(ftl.endurance, int(last[b.id] + last[b.id] - prediction[b.id]))
                            ftl.erase_counts = ftl.erase_counts + (prediction[b.id] - b.erase_counts)
                            b.erase_counts = prediction[b.id]

                        file = open(outfile, "a")
                        for ec in prediction:
                            file.write(str(ec) + " ")
                        file.write("\n")
                        file.close()
                        runs = runs - 1
                        if runs == 0:
                        	exit()                        	                    	                                         
                        ftl.build_heap()
                            
