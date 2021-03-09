import random

def getLba(r, f, U, Np):
    a = random.randint(0, f * U * Np - 1)
    b = random.randint(f * U * Np, U * Np - 1)
    if random.random() < r:
        return a
    else:
        return b

