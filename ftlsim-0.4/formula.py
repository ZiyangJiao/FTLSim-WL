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
#     r  : The fraction of the total write rate destined for hot pages
#     f  : these hot pages represent a fraction f of the overall address space
#     Np : the number of pages per block
#
# functions:
#     wl_worstcase_d        : happens when / under the assumption that all external writes target at hot blocks (r = 1.0 and f ~= 0)
#     wl_worstcase_r        : estimation for randomized WL overhead
#
def wl_worstcase_d(sf, Th):
    res = (1-sf) / (Th+1)
    print(res)
    return res

def wl_worstcase_r(sf, Th, Np):
    res = Np * (1-sf) / Th
    print(res)
    return res
