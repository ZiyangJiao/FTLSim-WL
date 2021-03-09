
       SSSL-ftlsim -- a fast Flash Translation Layer simulator

		   Peter Desnoyers, pjd@ccs.neu.edu
		      Solid-State Storage Lab
		Northeastern University Computer Science

Version 0.1, May 14 2012

SSSL-ftlsim is a set of Python extensions, implemented in C using the
SWIG interface generator, for generating traffic distributions
(getaddr) and running Flash Translation Layer simulations (runsim). 

These are stripped-down simulations which only handle single-page
writes, and which ignore all performance parameters other than write
amplification. High performance is achieved by implementing the
simulation engine and traffic generators in C -- e.g. page-mapped
greedy cleaning runs at over 10 million flash writes/sec on a 3.2GHz
Core i7. Implementing these different parts as Python extensions makes
it simple to configure the myriad simulation parameters, as well as
allowing e.g. composition of traffic generators.

A simulation consists of traffic generation, FTL simulation, and
statistics collection:

getaddr: flexible traffic generation.

This module supports the following traffic generator types, each of
which generate a stream of page addresses:

1. getaddr.uniform(max) 

This generates addresses uniformly distributed on [0..max-1]

2. m = getaddr.mixed()
This combines several generators probabilistically, e.g. to create
simple hot/cold traffic from two uniform generators. In particular:

  m.add(gen_i.handle, p_i, base_i)  for i = 1 .. n

will return (base_i + gen_i.next()) with probability (p_i - p_(i-1)),
where p_0 = 0. The user is responsible for maintaining resonable
invariants:
      base_1 = 0                      - addresses start at zero
      base_(i+1) - base_i = gen_i.max - ranges are independent
      p_n                             - total probability = 1

Remember to set gen_i.thisown = 0 so that Python doesn't garbage
collect the object after it goes out of scope.

3. getaddr.trace(file)

File contains lines of the form '<addr> <len>'. For each pair 'A N'
the address generator will return N addresses: A, A+1,... A+N-1. 
Numbers can be decimal or hex in 0x... format.

4. m = getaddr.scramble(max)
   m.input = <generator>.handle

Applies a random permutation to addresses from <generator> to remove
any spatial locality.

genaddr.py - python version of traffic generator

lambertw - the Lambert W function, for calculating optimal cleaning

ftlsim - FTL simulator, consisting of the FTL itself (basically the
reverse LBA-to-physical block map and a freelist), segments, and one
or more independently garbage-collected "pools". Types are:

segment: a physical flash block
  ftlsim.segment(Np) - constructor
  segment.n_valid - number of valid pages in block
  segment.page(i) - accesses array [0..Np-1] of LBAs (-1 = invalid)

ftl: FTL instance. It holds a single reverse map [lba to segment,page], 
     and a set of pools, each of which has a write frontier plus
     additional blocks

  ftlsim.ftl(U, Np) - creates a new FTL instance with a reverse map for
       	     U total segments, with segment size Np.

  ftl.int_writes, .ext_writes - internal and external write count
  ftl.nfree, .minfree - current number of free segments, target minimum #
  ftl.get_input_pool - which pool to write to. possible values are:
  		  write_select_first (*default)
		  write_select_top_down
		  write_select_python - evaluates 'get_input_pool_arg'
  ftl.get_pool_to_clean - which one to clean a segment from. Values:
  		  clean_select_first (*default)
		  clean_select_python - evaluates 'get_pool_to_clean_arg'

  Note that the default behavior is what you want for naive cleaning.

return_pool():
  Due to SWIG limitations, Python callbacks 'get_input_pool_arg' and
  'get_pool_to_clean_arg' don't provide their result as a return value,
  but instead pass it to 'return_pool'

  ftl.put_blk(), ftl.get_blk() - directly maintain the ftl freelist. On
	  	    startup you need to create T segments, set
	  	    .thisown = False on each, and pass them to
	  	    ftl.put_blk() before running the simulation. 

  ftl.run(addrgen, count) - run using addresses from 'addrgen' for 'count'
  	     	   iterations. If there are free blocks from
  	     	   ftl.put_blk() it will use them; otherwise cleaning
  	     	   is parameterized above.

Only needed for replacing ftl.run():
  ftl.find_blk(lba) - returns segment object for physical block holding 'lba'
  ftl.find_page(lba) - find the corresponding page number in the block
  	         (how do I get SWIG to return a pair?)

ftlsim.pool: a write frontier and associated pool of segments
  ftlsim.pool(ftl, type, Np) - create a pool associated with FTL instance
	  	'ftl'. 'type' is "lru" or "greedy"

  pool.frontier, .i - write frontier, next page to write in frontier segment
  pool.pages_valid, .pages_invalid - total valid and invalid pages, not
  		  counting the write frontier
  pool.next_pool - where cleaned pages from this pool get written
  pool.rate - EWMA of relative write rate to this pool (out of
  	  1.0). Smoothing constant can be adjusted by changing
  	  'ewma_rate' (default 0.95)

  pool.add_segment(segment) - add a segment to a pool. Need to do this for
		 the first segment; after that it should be handled
		 within run()

  pool.insert_segment(segment) - move a segment containing valid and invalid
                 pages to a pool, bypassing the write process. Used for
                 windowed greedy algorithm.

Note that the following two are only used if you're replacing
ftl.run() with Python code.

  ftl.remove_segment() - get a segment for cleaning according to the pool
		 policy. (e.g. LRU or Greedy)

  ftl.write(lba) - perform a write

Files:
  setup.py - Python build script
  ftlsim.i, ftlsim.c, ftlsim.h
  getaddr.c, getaddr.c, getaddr.h
  lambertw.i, lambertw.c
  genaddr.py

Installation:
  CFLAGS=-O3 python setup.py build
     and then either:
  python setup.py install
     or
  mv build/lib*/* .

Example files:
  low-level.py - direct Python implementation of cleaning, etc.
  greedy-high.py, lru-high.py - full-speed versions of naive Greedy
  		  	        and LRU cleaning
  windowed-greedy.py - algorithm from Hu 2009

  2hc.py - naive LRU cleaning with hot/cold data model
  3hc.py - naive LRU with 3-part data model

  two-pool.py - hot/cold data separation with global greedy cleaning

Error handling still isn't the best, and you may end up needing to use
GDB to figure out where your Python script went wrong.
