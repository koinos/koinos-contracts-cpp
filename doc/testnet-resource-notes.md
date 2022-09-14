
### Introduction

I (theoretical) wrote these design notes to describe my observations and suggestions for improvement of RC and resources on the testnet.

These notes discuss the state of the code and testnet as of September 14, 2022.  These notes were accurate when written, but will
be obsolete at some point.

### Decay constant doesn't match intended half-life

- Comments imply a 30-day half-life is intended.  Decay constant 18446694743881045523ull gives a 30-day half-life at 6 blocks / minute.
- This constant needs to be updated if we use a different block time target (3s), provided we still want a 30-day half-life.
- We may want a shorter half-lfie for testnet.

### Decay constant alternate implementation

- Instead of doing `x = (C*x) >> 64`, I instead recommend `x = x - ((D*x) >> b)` and then adjust `b` to be the largest possible value such that `D` fits in, say, 32 bits.
- For 10-second block times, I suggest implementing decay as `x = x-((0xb376037c * x) >> 51)`.
- For 3-second block times, I suggest implementing decay as `x = x-((0xd75a712f * x) >> 53)`.

### The current state of the code

Currently `update_market()` looks like this:

```
void update_market( market& m, uint64_t consumed )
{
   auto k = int128_t( m.resource_supply() ) * int128_t( m.rc_reserve() );
   m.set_resource_supply( m.resource_supply() + m.block_budget() - consumed );
   m.set_resource_supply( ( ( int128_t( m.resource_supply() ) * constants::decay_constant ) >> 64 ).convert_to< uint64_t >() );
   m.set_rc_reserve( ( ( k + ( m.resource_supply() - 1 ) ) / m.resource_supply() ).convert_to< uint64_t >() );
}
```

I personally think it is clearer to refactor this code to have one effect per line:

```
void update_market( market& m, uint64_t consumed )
{
   int128_t resource_supply = m.resource_supply();
   int128_t rc_reserve = m.rc_reserve();
   int128_t k = resource_supply * rc_reserve;

   resource_supply += m.block_budget();
   resource_supply -= consumed;               // TODO:  Check for overflow, or write a comment explaining caller must check for overflow
   resource_supply = (resource_supply * decay_constant) >> 64;

   rc_reserve = (k + (resource_supply - 1)) / resource_supply;

   m.set_resource_supply( resource_supply );
   m.set_rc_reserve( rc_reserve );
}
```

This code seems very confused in its conceptual model.  For example, a small portion of the budget immediately decays; this doesn't make much sense.

### Specifying the desired behavior

What should the conceptual model of RC and resources be?

Here is my recommended specification for RC and resources:

- (1) New mana (RC) enters the chain every second into user mana balances, as specified in `update_mana()` / `get_account_rc()`.
- (2) RC is traded to the system from user balances in exchange for resources, as implemented in `consume_account_rc` in `apply_block`, at per-unit rates and limits specified in `get_resource_limits()`.
- (3) Any RC subtracted from user balances in step (2) is added to the system RC balance `rc_reserve`.  Thus, in Step (2), resources leave the chain, but RC does not enter or leave the chain.
- (4) When a block occurs, the system RC balance decays.  This decay is the only means for RC to leave the system.
- (5) When a block occurs, the system resource balance `resource_supply` decays.  This decay allows resources leave the chain without being used by users, limiting the extent to which users can burst after a period of less-than-full activity.
- (6) Every block, a per-block resource budget is added to the system resource balance.  This is needed to prevent the chain from eventually running out of resources and effectively halting.
- (7) Resources entering the system balance from the budget do not decay until the next block.  Resources exiting the system balance due to user spend do not decay.
- (8) Every block, a per-block phantom RC spend is added to the system RC balance.  This prevents an unstable no-load condition where the price of resources crashes to zero when there is very little activity on the blockchain.
- (9) RC entering the system balance from user activity or phantom RC spend do not decay until the next block.
- (10) To add numerical stability and achieve an economically reasonable approximation of continuous behavior with all-integer arithmetic, per-unit resource rates should not be allowed to fall below 10000.
- (11) The blockchain should be initialized to an initial condition corresponding to twice the phantom RC spend.

### Spec deficiencies

Now that we have a clear model of what the code *should* do, many deficiencies of the code become clear:

- The code updates `rc_reserve` according to the xyk invariant, which fails (3).  The xyk invariant does not give the same price as specified in `get_resource_limits()` (except in the limiting case where maximum resource usage occurred; even in this limiting case, the calculation is further disturbed by the fact that the xyk invariant is only applied *after* the budget and decay).
- As far as I can tell, the code does not implement any decay of RC at all; this fails (4).
- The order of operations in the above pseudocode clearly fails (7).
- The code doesn't implement (8) either.
- Prices crashing to zero seems to be occurring on the testnet; this fails (9).
- On testnet, the `chain.get_resource_limits` gives `"compute_bandwidth_cost": "1"` which clearly fails (10).

### To fork or not to fork?

One problem is the following:  We clearly count resources as they are spent, since `update_market()` has access to this information.  We don't have access to RC in the same way, and changing `resource_meter` would be a hardfork.

- Option (A):  Update `resource_meter` to also count RC, create an incompatible release of `chain`, and launch a new testnet.
- Option (B):  Update `resource_meter` to also count RC, create a new `apply_block` thunk that passes the information to `consume_block_resources()`, and activate it via governance (i.e., practice for doing a hardfork on mainnet).
- Option (C):  Update `consume_account_rc` to update a running tally of RC usage.  This option comes with a major performance penalty (an extra state update per transaction).  Worse, I'm not sure this option is technically possible; I don't know if *anything* submits per-resource RC usage to the programmable system contracts.
- Option (D):  Reverse engineer the addition by having `update_market()` call `calculate_market_limit()` before updating any fields, which effectively gives `consume_block_resources()` access to the same effective cost users paid in step (2).

I recommend Option (D), as it's a straightforward in-band upgrade that avoids logistical hurdles.  (On the other hand, we may *want* to get some experience hardforking Koinos before mainnet launches, in which case Option (B) might be workable.)

### Suggested refactoring

I suggest the following refactoring:

```
#define DECAY_CONSTANT_MUL 0xd75a712f
#define DECAY_CONSTANT_SHIFT 53

#define PHANTOM_RC_CONSTANT_MUL 0xee9bfab5
#define PHANTOM_RC_CONSTANT_SHIFT 59

// Multiply by a number less than 1 using integer multiplication followed by a shift.
// The result is guaranteed not to overflow, and the final cast is guaranteed to fit,
// as long as x, m are 64-bit, x*m does not overflow 127 bits,
// and m, s represent multiplication by a number smaller than 1.
#define MUL_SHIFT(x, m, s) \
   uint64_t(((m) * int128_t(x)) >> (s))

#define DECAY(x, m, s) \
   x -= MUL_SHIFT(x, m, s);

void update_market( market& m, uint64_t resources_consumed )
{
   // Use price times quantity to back-calculate how much RC users spent.
   auto [limit, cost] = calculate_market_limit( m );
   int128_t rc_consumed = resources_consumed;
   rc_consumed *= cost;

   // Per (7), decay should apply after resource consumption and before budget.
   uint64_t resource_supply = m.resource_supply();

   // TODO:  Check for subtraction overflow, or write a comment explaining caller must check for overflow
   resource_supply -= resources_consumed;
   DECAY(resource_supply, DECAY_CONSTANT_MUL, DECAY_CONSTANT_SHIFT);
   resource_supply += m.block_budget();

   uint64_t rc_reserve = m.rc_reserve();

   // Per (9), RC decay should apply before phantom RC or user consumption.
   DECAY(rc_reserve, DECAY_CONSTANT_MUL, DECAY_CONSTANT_SHIFT);
   int128_t rc_reserve_128 = int128_t(rc_reserve) + rc_consumed;

   auto koin_token = koinos::token( constants::koin_contract );

   uint64_t phantom_rc = koin_token.total_supply();
   phantom_rc = MUL_SHIFT( phantom_rc, PHANTOM_RC_CONSTANT_MUL, PHANTOM_RC_CONSTANT_SHIFT );
   rc_reserve_128 += phantom_rc;
   rc_reserve = uint64_t( std::min( (int128_t(1) << 64)-1, rc_reserve_128 ) );

   m.set_resource_supply( resource_supply );
   m.set_rc_reserve( rc_reserve );
}
```

### The phantom whale

Phantom RC can be thought of as the actions of a *phantom whale* who, every block, spends
some RC for every resource, but then (using a magical power unavailable to normal users)
immediately sends the resources back to the system resource balance instead of using it.
Thus, the phantom whale's activity continually increases the RC balance, and drives it
away from the numerical instability at zero.

With the constants in the above listing, when the supply is 100M KOIN, the phantom whale
transacts with `0.69444444` KOIN.

I did not specify the constant `0.69444444` directly.  Instead, I specified that the phantom
whale's balance (per resource) should be 0.1% of the supply, or 100,000 KOIN (when the supply is
100M).

One satoshi of RC is one satoshi-gent of KOIN-time, where the time unit gent is
`mana_regen_time_ms = 432'000'000` milliseconds.  This means:

- The RC generated per millisecond across the entire blockchain is `koin.total_supply() / mana_regen_time_ms`
- The RC generated per block interval is `koin.total_supply() * block_interval / mana_regen_time_ms`

We multiply this by the desired phantom RC balance (as a percentage of supply, e.g. `0.1%`), and obtain
phantom RC constants corresponding to multiplication by `0.001 * 3000 / 432e6 = 6.9444444e-09`.  For a
supply of 100M, this corresponds to 0.69444444 KOIN transacting at max RC, per resource, per block.

In order to be able to indefinitely sustain its rate of transaction for a single resource, the phantom
whale would have to have a fresh 0.69444444 KOIN already at max capacity for every block in the entire 5-day
regeneration period; its total balance would have to be `20*60*24*5*0.69444444 ~= 100,000 KOIN`.
This number is `0.1%` of the supply (which was of course guaranteed by the way we calculated 0.69444444).

As VHP doesn't get mana / RC, the phantom whale's activity should scale with KOIN only, not VHP + KOIN.
This means, in a no-load condition, net burning of KOIN for VHP makes transacting cheaper for everyone,
exactly offsetting the monetary deflation of KOIN; and the reverse effect happens when
there is net printing of KOIN via block rewards.

### No-load equilibrium

Calculating the no-load equilibrium is a straightforward exercise in algebra.  The final result is:

```
gent_per_block = 3000.0 / 432e6
rc_per_mana = 1
rc_reserve = (phantom_rc_mult + utilized_supply * gent_per_block) * koin_supply * rc_per_mana / decay_mult
rc_per_block = utilized_supply * gent_per_block * koin_supply * rc_per_mana
resource_supply = block_budget / ((rc_per_block / rc_reserve) + decay)
price = rc_reserve*1.0e8 / resource_supply
```

Here is a report from a simple Python script that computes the equilibrium at varying loads for the
budgets currently in effect on the testnet:

```
Budget is: 39600
At 0.0000 utilization, resource_supply=    98721910216   rc_reserve=   1731234   price=1753.6471855256063
At 0.0010 utilization, resource_supply=    65814606811   rc_reserve=   3462468   price=5260.941556550174
At 0.0020 utilization, resource_supply=    59233146130   rc_reserve=   5193703   price=8768.237615812759
At 0.0050 utilization, resource_supply=    53848314663   rc_reserve=  10387406   price=19290.122755016037
At 0.0100 utilization, resource_supply=    51711476780   rc_reserve=  19043578   price=36826.598631128865
At 0.1000 utilization, resource_supply=    49606531999   rc_reserve= 174854674   price=352483.1649257901
At 0.2500 utilization, resource_supply=    49459479968   rc_reserve= 434539833   price=878577.4401209733
At 0.5000 utilization, resource_supply=    49410266751   rc_reserve= 867348432   price=1755401.2334540696
At 0.7500 utilization, resource_supply=    49393840487   rc_reserve=1300157031   price=2632225.0268071163
At 0.9000 utilization, resource_supply=    49388362634   rc_reserve=1559842191   price=3158319.3040017313
At 0.9900 utilization, resource_supply=    49385872299   rc_reserve=1715653286   price=3473975.8682661555
Budget is: 262144
At 0.0000 utilization, resource_supply=   653519101811   rc_reserve=   1731234   price=264.90947168988475
At 0.0010 utilization, resource_supply=   435679401211   rc_reserve=   3462468   price=794.7284150629657
At 0.0020 utilization, resource_supply=   392111461089   rc_reserve=   5193703   price=1324.5476134708424
At 0.0050 utilization, resource_supply=   356464964625   rc_reserve=  10387406   price=2914.0047496470006
At 0.0100 utilization, resource_supply=   342319529521   rc_reserve=  19043578   price=5563.100073971021
At 0.1000 utilization, resource_supply=   328385220313   rc_reserve= 174854674   price=53246.81599048138
At 0.2500 utilization, resource_supply=   327411765578   rc_reserve= 434539833   price=132719.67555377257
At 0.5000 utilization, resource_supply=   327085984023   rc_reserve= 867348432   price=265174.4416963491
At 0.7500 utilization, resource_supply=   326977245476   rc_reserve=1300157031   price=397629.20783899963
At 0.9000 utilization, resource_supply=   326940983193   rc_reserve=1559842191   price=477102.06770840747
At 0.9900 utilization, resource_supply=   326924497675   rc_reserve=1715653286   price=524785.7833234491
Budget is: 57500000
At 0.0000 utilization, resource_supply=143346208016057   rc_reserve=   1731234   price=1.2077291921151307
At 0.0010 utilization, resource_supply= 95564138678271   rc_reserve=   3462468   price=3.6231875763112824
At 0.0020 utilization, resource_supply= 86007724810282   rc_reserve=   5193703   price=6.038647123216433
At 0.0050 utilization, resource_supply= 78188840736366   rc_reserve=  10387406   price=13.285023671119308
At 0.0100 utilization, resource_supply= 75086108960975   rc_reserve=  19043578   price=25.362318361572903
At 0.1000 utilization, resource_supply= 72029686615054   rc_reserve= 174854674   price=242.75362314773125
At 0.2500 utilization, resource_supply= 71816164095877   rc_reserve= 434539833   price=605.0724631015862
At 0.5000 utilization, resource_supply= 71744705510538   rc_reserve= 867348432   price=1208.9371972857316
At 0.7500 utilization, resource_supply= 71720854243879   rc_reserve=1300157031   price=1812.8019314702483
At 0.9000 utilization, resource_supply= 71712900290101   rc_reserve=1559842191   price=2175.120772817656
At 0.9900 utilization, resource_supply= 71709284272547   rc_reserve=1715653286   price=2392.5120762316915
```

As is clear from the equilbria, we still fail (10).  The solution however is simple:  Rescale either mana or
RC by a factor of 10,000.  I chose to rescale RC per mana, as rescaling mana decreases the user balance
level at which overflow occurs (and thus, maximizes the room for the KOIN supply to expand before it
needs a redenomination or other major upgrade due to running out of bits).

We also need to multiply `phantom_rc` by 10000, resulting in new constants:

```
#define PHANTOM_RC_CONSTANT_MUL 0x91a2b3c5
#define PHANTOM_RC_CONSTANT_SHIFT 45
```

Re-running the equilibrium script gives us the initialization balances for `resource_supply` and `rc_reserve`
with the rescaled RC:

```
At 0.0010 utilization, resource_supply=    65814606811   rc_reserve=34624687927   price=52609427.609940484     // disk
At 0.0010 utilization, resource_supply=   435679401211   rc_reserve=34624687927   price=7947285.970086805      // network
At 0.0010 utilization, resource_supply= 95564138678271   rc_reserve=34624687927   price=36231.88405806542      // compute
```
