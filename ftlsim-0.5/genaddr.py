#
# file:        genaddr.py
# description: python implementation of FTL simulator input
#

import random


class genaddr:
    def __init__(self):
        self.eof = False
        
    def __iter__(self):
        return self.addrs()
    
    def addrs(self):
        while True:
            yield self.next()

    def next_n(self, n):
        while True:
            if n < 1:
                break
            n -= 1
            yield self.next()


# basic uniform random address generator
#
class uniform(genaddr):
    def __init__(self, max):
        genaddr.__init__(self)
        self.max = max

    def addrs(self):
        while True:
            yield(random.randint(0, self.max-1))

    def next(self):
        return random.randint(0, self.max-1)

    def next10(self):
        return [random.randint(0, self.max-1) for i in range(10)]

class seq(genaddr):
    def __init__(self):
        genaddr.__init__(self)
        self.next = 0

    def addrs(self):
        while True:
            yield(self.next())

    def next(self):
        tmp = self.next;
        self.next += 1
        return tmp
    
# probabilistic mix of multiple sources
# use with genaddr.uniform to generate e.g. hot/cold mixes
#
class mixed(genaddr):
    def __init__(self, *args):
        genaddr.__init__(self)
        self.generators = []
        p,base = 0,0
        for _p,_gen in args:
            p += _p
            self.generators.append(p, base, _gen)
            base += _gen.max

    def next(self):
        val = random.random()
        for p,base,gen in self.generators:
            if val <= p:
                return base + gen.next()

# parse input from a file
# Note that 'file' can be any iterator that returns strings, or if
# it's a filename it will be opened.
#
class trace(genaddr):
    def __init__(self, file):
        genaddr.__init__(self)
        if type(file) == str:
            file = open(file, "r")
        self.fp = file
        self.count = 0
        self.addr = 0
        self.iter = self._iter()

    def next(self):
        return self.iter.next()

    def addrs(self):
        return self.iter
    
    def _iter(self):
        for l in self.fp:
            vals = l.split()
            (a,n) = map(int, vals)
            for i in range(a, a+n):
                yield(i)
        self.eof = True

# shuffle input in the space domain.
# input must be integer addresses in 0..max-1
#
class scramble(genaddr):
    def __init__(self, src, max):
        genaddr.__init__(self)
        self.max = max
        self.src = src
        self.permut = [i for i in range(max)]
        random.shuffle(self.permut)
        self.iter = self._iter()

    def next(self):
        return self.iter.next()

    def _iter(self):
        for a in self.src.addrs():
            yield self.permut[a]
        self.eof = True
        
    def next(self):
        return self.iter.next()

def head(n, iter):
    for i in range(n):
        pass

# shuffle input in the time domain.
# Note that this works on any iterator, so we can apply it to lines in
# a file before parsing them with genaddr.trace 
#
class shuffle(genaddr):
    def __init__(self, src, n):
        genaddr.__init__(self)
        self.src = src
        self.n = n
        self.buffer = []
        self.iter = self._iter()

    def _iter(self):
        for a in self.src:
            self.buffer.append(a)
            if len(self.buffer) >= self.n:
                random.shuffle(self.buffer)
                while self.buffer:
                    yield self.buffer.pop()
        random.shuffle(self.buffer)
        while self.buffer:
            yield self.buffer.pop()
        self.eof = True

    def next(self):
        return self.iter.next()
