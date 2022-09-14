#!/usr/bin/env python3

decay_mult = 0xd75a712f / 2.0**53
phantom_rc_mult = 0xee9bfab5 / 2.0**59

koin_supply = 100e6

disk_budget_per_block          = 39600        # 10G per month
max_disk_per_block             = 200 << 10    # 200k
network_budget_per_block       = 1 << 18      # 256k block
max_network_per_block          = 1 << 20      # 1M block
compute_budget_per_block       = 57500000     # ~0.1s
max_compute_per_block          = 287500000    # ~0.5s

rc_per_mana = 10000

gent_per_block = 3000.0 / 432e6     # (3000 ms/block) / 432e6 (ms/gent) = 6.94444444e-06 gent/block

# At equilibrium for load L, (phantom_rc_mult + utilized_supply * gent_per_block) * koin_supply * rc_per_mana = decay_mult * rc_reserve
def calc_eq_rc_reserve(utilized_supply):
    return (phantom_rc_mult + utilized_supply * gent_per_block) * koin_supply * rc_per_mana / decay_mult

rc_reserve = calc_eq_rc_reserve(0.0)

# resources_per_block + decay*resource_supply = block_budget
# resources_per_block = price*rc_per_block
# rc_per_block = utilized_supply * gent_per_block * koin_supply * rc_per_mana
# price*rc_per_block + decay*resource_supply = block_budget
# price = resource_supply / rc_reserve
# resource_supply * rc_per_block / rc_reserve + decay * resource_supply = block_budget
# resource_supply = block_budget / ((rc_per_block / rc_reserve) + decay)

def calc_eq_resource_supply(block_budget, utilized_supply):
    rc_reserve = calc_eq_rc_reserve(utilized_supply)
    rc_per_block = utilized_supply * gent_per_block * koin_supply * rc_per_mana
    return block_budget / ((rc_per_block / rc_reserve) + decay_mult)

util_points = [0.0, 0.001, 0.002, 0.005, 0.01, 0.10, 0.25, 0.5, 0.75, 0.9, 0.99]

for budget in [disk_budget_per_block, network_budget_per_block, compute_budget_per_block]:
    print(f"Budget is: {budget}")
    for u in util_points:
        i_rc = int(calc_eq_rc_reserve(u))
        i_res = int(calc_eq_resource_supply(budget, u))
        p = i_rc*1.0e8 / i_res
        print(f"At {u:.04f} utilization, resource_supply={i_res:15}   rc_reserve={i_rc:10}   price={p}")
