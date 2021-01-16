#
# file:        formula.py
# description: formulas that provides amplification upper bound for SSD wear leveling policy
#
# Ziyang Jiao, Syracuse University, 2021
#
# Copyright 2021 Ziyang Jiao
#
# parameters:
#     sf : spare factor (T-U)/T --- "Analytic Modeling of SSD Write Performance, Peter Desnoyers"
#     Th : user-configurable parameter to trigger wear leveling --- "On Efficient Wear Leveling for Large Scale FlashMemory Storage Systems, Li-Pin Chang"
#     r = The fraction of the total write rate destined for hot pages
#     f = these hot pages represent a fraction f of the overall address space
#
# functions:
#     wl_worstcase        : happens when / under the assumption that all external writes target at hot blocks (r = 1.0 and f ~= 0)
#     wl_aggressivebound  : incorporate parameters sf, r, Th, providing an aggressive bound, assuming f >> 1 - f
#     wl_generalbound     : incorporate parameters sf, r, f, Th, assuming OP(over-provisioning) region are cold blocks to generate a conservative bound
#     wl_generalbound1    : incorporate parameters sf, r, f, Th, assuming cold blocks are (1-f) * U instead of T - f * U as in "wl_generalbound"
#
def wl_worstcase(sf, Th):
    res = 2.0 * (1-sf) / (Th+1)
    print(res)
    return res

def wl_aggressivebound(sf, r, Th):
    res = (2.0 * (1-sf) * (2*r-1)) / (Th+1)
    print(res)
    return res

def wl_generalbound(sf, r, f, Th):
    res = (2.0 * (1-sf) * ((r/(1-sf)) - f)) / ((Th+1) * ((1/(1-sf)) - f))
    print(res)
    return res

def wl_generalbound1(sf, r, f, Th):
    res = (2.0 * (1-sf) * (r - f)) / ((Th+1) * (1 - f))
    print(res)
    return res

# wl_worstcase(0.05, 3)
# wl_generalbound(0.05, 0.9, 0.1, 3)
# wl_generalbound(0.05, 0.8, 0.2, 3)
wl_generalbound(0.05, 0.5, 0.5, 3)
wl_generalbound1(0.05, 0.5, 0.5, 3)

# wl_generalbound1(0.2, 1, 1, 3)
# wl_generalbound(0.2, 0.8, 0.2, 3)
# wl_generalbound(0.2, 0.7, 0.3, 5)
