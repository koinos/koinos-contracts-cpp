#!/usr/bin/env python3

from math import log, expm1

def compute_shift(r, bits):
    m = 1 << bits
    p = 1
    shift = 0
    while r*p < m:
        p <<= 1
        shift += 1
    return shift-1

"""
Given floating point r, compute integers m, s such that
(x*m) >> s is approximately equal to x*r.  And m fits in
a value with the given number of bits.
"""
def compute_mul_shift(r, bits):
    s = compute_shift(r, bits)
    return (int(0.5 + r * (1 << s)), s)

def compute_mul_shift_for_halflife(hl, bits):
    #    (1-r)^n = 0.5
    # -> n log(1-r) = -log(2)
    # -> log(1-r) = -log(2)/n
    # -> 1-r = exp(-log(2)/n)
    # -> -r = exp(-log(2)/n) - 1
    # -> r = -expm1(-log(2)/n)

    r = -expm1(-log(2)/hl)
    return compute_mul_shift(r, bits)

def format_mul_shift(ms):
    m, s = ms
    return f"({m:x}, {s})"

print(format_mul_shift(compute_mul_shift_for_halflife(       1000, 32)))
print(format_mul_shift(compute_mul_shift_for_halflife( 6*60*24*30, 32)))
print(format_mul_shift(compute_mul_shift_for_halflife(20*60*24*30, 32)))
print(format_mul_shift(compute_mul_shift(.001 * 3000 / 432e6, 32)))
print(format_mul_shift(compute_mul_shift(10000 * .001 * 3000 / 432e6, 32)))
