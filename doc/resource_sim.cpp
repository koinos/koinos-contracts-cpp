
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <utility>

#define RC_PER_MANA 10000
#define BLOCKS_PER_GENT (20*60*24*5)

#define DECAY_CONSTANT_MUL 0xd75a712f
#define DECAY_CONSTANT_SHIFT 53

#define PHANTOM_RC_CONSTANT_MUL 0x91a2b3c5
#define PHANTOM_RC_CONSTANT_SHIFT 45

// May need some other int128_t support on non-GCC compilers
typedef __int128 int128_t;

// From https://stackoverflow.com/questions/25114597/how-to-print-int128-in-g
// Modified to handle negative values
std::string int128_to_string(__int128 num)
{
    std::string str;
    if( num < 0 ) { return "-" + int128_to_string(-num); }
    do {
        int digit = num % 10;
        str = std::to_string(digit) + str;
        num = (num - digit) / 10;
    } while (num != 0);
    return str;
}

struct market
{
   uint64_t _resource_supply = 0;
   uint64_t _block_limit = 0;
   uint64_t _rc_reserve = 0;
   uint64_t _block_budget = 0;

   uint64_t resource_supply()const { return _resource_supply; }
   uint64_t block_limit()    const { return _block_limit;     }
   uint64_t rc_reserve()     const { return _rc_reserve;      }
   uint64_t block_budget()   const { return _block_budget;    }

   void set_resource_supply(uint64_t resource_supply)
   { _resource_supply = resource_supply; }
   void set_rc_reserve(uint64_t rc_reserve)
   { _rc_reserve = rc_reserve; }

   market( uint64_t resource_supply, uint64_t block_limit, uint64_t rc_reserve, uint64_t block_budget )
      : _resource_supply(resource_supply),
        _block_limit(block_limit),
        _rc_reserve(rc_reserve),
        _block_budget(block_budget)
   {}
};

class blockchain_mock
{
   public:
      blockchain_mock(uint64_t koin_supply)
         : _koin_supply(koin_supply),
           _mana(koin_supply)
      {}

      uint64_t koin_supply() const
      { return _koin_supply; }

      uint64_t get_mana_percent(uint64_t supply_percent)const
      {
         uint64_t mana_percent = uint64_t( int128_t(_koin_supply) * supply_percent / (uint64_t( 10000 ) * BLOCKS_PER_GENT) );
         return std::min(mana_percent, _mana);
      }

      void spend_mana(uint64_t mana_spent)
      {
         _mana -= mana_spent;
      }

      void regen_mana()
      {
         _mana += _koin_supply / BLOCKS_PER_GENT;
         _mana = std::min( _mana, _koin_supply );
      }

      uint64_t _koin_supply = 0;
      uint64_t _mana = 0;
};

std::pair< uint64_t, uint64_t > calculate_market_limit( const market& m )
{
   auto resource_limit = std::min( m.resource_supply() - 1, m.block_limit() );
   auto k = int128_t( m.resource_supply() ) * int128_t( m.rc_reserve() );
   // std::cout << "res: " << m.resource_supply() << "  rc: " << m.rc_reserve() << std::endl;
   // std::cout << "k: " << int128_to_string(k) << std::endl;
   auto new_supply = m.resource_supply() - resource_limit;
   auto consumed_rc = ( ( k + ( new_supply - 1 ) ) / new_supply ) - m.rc_reserve();
   // std::cout << "consumed_rc: " << uint64_t(consumed_rc) << std::endl;
   // std::cout << "resource_limit: " << uint64_t(resource_limit) << std::endl;
   auto rc_cost = uint64_t( ( consumed_rc + ( resource_limit - 1 ) ) / resource_limit );
   // std::cout << "rc_cost: " << rc_cost << std::endl;
   return std::make_pair( resource_limit, rc_cost );
}

// Multiply by a number less than 1 using integer multiplication followed by a shift.
// The result is guaranteed not to overflow, and the final cast is guaranteed to fit,
// as long as x, m are 64-bit, x*m does not overflow 127 bits,
// and m, s represent multiplication by a number smaller than 1.
#define MUL_SHIFT(x, m, s) \
   uint64_t(((m) * int128_t(x)) >> (s))

#define DECAY(x, m, s) \
   x -= MUL_SHIFT(x, m, s);

void update_market( const blockchain_mock& chain, market& m, uint64_t resources_consumed )
{
   // Use price times quantity to back-calculate how much RC users spent.
   auto [limit, cost] = calculate_market_limit( m );
   int128_t rc_consumed = resources_consumed;
   rc_consumed *= cost;

   // Per (7), decay should apply after resource consumption and before budget.
   uint64_t resource_supply = m.resource_supply();

   // TODO:  Check for subtraction overflow, or write a comment explaining caller must check for overflow
   // std::cout << "in update_market()" << std::endl;
   // std::cout << "   resource_supply initially " << resource_supply << std::endl;
   resource_supply -= resources_consumed;
   // std::cout << "   " << resources_consumed << " resources consumed by user" << std::endl;
   uint64_t begin_resource = resource_supply;
   DECAY(resource_supply, DECAY_CONSTANT_MUL, DECAY_CONSTANT_SHIFT);
   uint64_t end_resource = resource_supply;
   // std::cout << "   " << (begin_resource-end_resource) << " resources lost to decay" << std::endl;
   resource_supply += m.block_budget();
   // std::cout << "   " << m.block_budget() << " resources added to budget" << std::endl;
   // std::cout << "   resource supply is now " << resource_supply << std::endl;

   uint64_t rc_reserve = m.rc_reserve();

   // Per (9), RC decay should apply before phantom RC or user consumption.
   uint64_t begin_rc = rc_reserve;
   DECAY(rc_reserve, DECAY_CONSTANT_MUL, DECAY_CONSTANT_SHIFT);
   uint64_t end_rc = rc_reserve;
   // std::cout << "   " << (begin_rc-end_rc) << " RC lost to decay" << std::endl;
   int128_t rc_reserve_128 = int128_t(rc_reserve) + rc_consumed;
   // std::cout << "   " << int128_to_string(rc_consumed) << " RC added via consumption" << std::endl;

   // auto koin_token = koinos::token( constants::koin_contract );

   uint64_t phantom_rc = chain.koin_supply();
   phantom_rc = MUL_SHIFT( phantom_rc, PHANTOM_RC_CONSTANT_MUL, PHANTOM_RC_CONSTANT_SHIFT );
   rc_reserve_128 += phantom_rc;
   // std::cout << "   " << phantom_rc << " RC added (phantom)" << std::endl;
   rc_reserve = uint64_t( std::min( (int128_t(1) << 64)-1, rc_reserve_128 ) );

   m.set_resource_supply( resource_supply );
   m.set_rc_reserve( rc_reserve );
}

constexpr uint64_t disk_budget_per_block          = 39600; // 10G per month
constexpr uint64_t max_disk_per_block             = 200 << 10; // 200k
constexpr uint64_t network_budget_per_block       = 1 << 18; // 256k block
constexpr uint64_t max_network_per_block          = 1 << 20; // 1M block
constexpr uint64_t compute_budget_per_block       = 57'500'000; // ~0.1s
constexpr uint64_t max_compute_per_block          = 287'500'000; // ~0.5s

constexpr uint64_t disk_initial_resource_supply = 65814606811;
constexpr uint64_t network_initial_resource_supply = 435679401211;
constexpr uint64_t compute_initial_resource_supply = 95564138678271;

constexpr uint64_t disk_initial_rc_reserve = 34624687927;
constexpr uint64_t network_initial_rc_reserve = 34624687927;
constexpr uint64_t compute_initial_rc_reserve = 34624687927;

constexpr uint64_t sat = 1'0000'0000;

int main(int argc, char** argv, char** envp)
{
   blockchain_mock chain( 100'000'000 * sat );
   market disk_market(disk_initial_resource_supply, max_disk_per_block, disk_initial_rc_reserve * sat, disk_budget_per_block);
   market network_market(network_initial_resource_supply, max_network_per_block, network_initial_rc_reserve * sat, network_budget_per_block);
   market compute_market(compute_initial_resource_supply, max_compute_per_block, compute_initial_rc_reserve * sat, compute_budget_per_block);

   uint64_t sim_months = 12;

   uint64_t mana_spend_percent_bp = 500;   // spend 5% of mana per block divided equally among 3 resources

   for( int i=0; i<20*60*24*30*sim_months; i++ )
   {
      uint64_t mana_spent = chain.get_mana_percent(mana_spend_percent_bp);

      uint64_t disk_mana_spent    = mana_spent / 3;
      uint64_t network_mana_spent = mana_spent / 3;
      uint64_t compute_mana_spent = mana_spent / 3;

      auto [disk_limit,    disk_cost]    = calculate_market_limit( disk_market );
      auto [network_limit, network_cost] = calculate_market_limit( network_market );
      auto [compute_limit, compute_cost] = calculate_market_limit( compute_market );

      uint64_t disk_used = std::min( disk_limit, (disk_mana_spent * RC_PER_MANA) / disk_cost);
      uint64_t network_used = std::min( network_limit, (network_mana_spent * RC_PER_MANA) / network_cost );
      uint64_t compute_used = std::min( compute_limit, (compute_mana_spent * RC_PER_MANA) / compute_cost );

      disk_mana_spent    = (disk_used * disk_cost + (RC_PER_MANA - 1)) / RC_PER_MANA;
      network_mana_spent = (network_used * network_cost + (RC_PER_MANA - 1)) / RC_PER_MANA;
      compute_mana_spent = (compute_used * compute_cost + (RC_PER_MANA - 1)) / RC_PER_MANA;

      if( i%100000 == 0 )
      {
         std::cout << "block " << i << std::endl;
         std::cout << "price=[" << disk_cost << ", " << network_cost << ", " << compute_cost << "]" << std::endl;
         std::cout << "usage=[" << disk_used << ", " << network_used << ", " << compute_used << "]" << std::endl;
         std::cout << "mana_spent=[" << disk_mana_spent << ", " << network_mana_spent << ", " << compute_mana_spent << "]" << std::endl;
         std::cout << "rc=[" << disk_market.rc_reserve() << ", " << network_market.rc_reserve() << ", " << compute_market.rc_reserve() << "]" << std::endl;
         std::cout << "res=[" << disk_market.resource_supply() << ", " << network_market.resource_supply() << ", " << compute_market.resource_supply() << "]" << std::endl;
      }

      chain.spend_mana( disk_mana_spent + network_mana_spent + compute_mana_spent );

      update_market(chain, disk_market, disk_used);
      update_market(chain, network_market, network_used);
      update_market(chain, compute_market, compute_used);

      chain.regen_mana();
   }
   std::cerr << "\n";

   return 0;
}
