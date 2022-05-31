# FTLSim-WL
FTLSim extension with a serious of wear leveling, garbage collection, block allocation policies.

The orginal FTLSim is the simulator used in "Analytic Modeling of SSD Write Performance, Peter Desnoyers".

SSSL-ftlsim is a set of Python extensions, implemented in C using the SWIG interface generator, for generating traffic distributions (getaddr) and running Flash Translation Layer simulations (runsim). 

These are stripped-down simulations which only handle single-page writes, and which ignore all performance parameters other than write amplification. High performance is achieved by implementing the simulation engine and traffic generators in C -- e.g. page-mapped greedy cleaning runs at over 10 million flash writes/sec on a 3.2GHz Core i7. Implementing these different parts as Python extensions makes it simple to configure the myriad simulation parameters, as well as allowing e.g. composition of traffic generators.

## Three main components
A simulation consists of traffic generation, FTL simulation, and statistics collection.

## Files
  ```
  setup.py - Python build script  
  ftlsim.i, ftlsim.c, ftlsim.h
  getaddr.i, getaddr.c, getaddr.h
  lambertw.i, lambertw.c
  genaddr.py
  ```
## Installation
  ```
  cp target_ftl_congiguration_file.c ftlsim.c
  CFLAGS=-O3 python2 setup.py build
     and then either:
  python2 setup.py install
     or
  mv build/lib*/* 
  ```  
## Example files:
  ```
  low-level.py - direct Python implementation of cleaning, etc.
  greedy-high.py, lru-high.py - full-speed versions of naive Greedy and LRU cleaning
  windowed-greedy.py - algorithm from Hu 2009

  2hc.py - naive LRU cleaning with hot/cold data model
  3hc.py - naive LRU with 3-part data model

  two-pool.py - hot/cold data separation with global greedy cleaning
  ```
  
Error handling still isn't the best, and you may end up needing to use GDB to figure out where your Python script went wrong.

