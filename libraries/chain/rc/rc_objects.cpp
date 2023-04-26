#include <hive/chain/rc/rc_objects.hpp>

namespace hive { namespace chain {

void rc_stats_object::archive_and_reset_stats( rc_stats_object& archive, const rc_pool_object& pool_obj,
  uint32_t _block_num, int64_t _regen )
{
  //copy all data to archive (but not the id)
  {
    auto _id = archive.id;
    archive = copy_chain_object();
    archive.id = _id;
  }

  block_num = _block_num;
  regen = _regen;
  budget = pool_obj.get_last_known_budget();
  pool = pool_obj.get_pool();
  for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
    share[i] = pool_obj.count_share(i);
  for( int i = 0; i < 3; ++i )
    if( op_stats[i].count > 0 ) //leave old value if there is no new data
      average_cost[i] = op_stats[i].average_cost();

  op_stats = {};
  payer_stats = {};
}

void rc_stats_object::add_stats( const rc_info& tx_info )
{
  int _op_idx = op_stats.size() - 1; //multiop transaction by default
  if( tx_info.op.valid() )
    _op_idx = tx_info.op.value();
  rc_op_stats& _op_stats = op_stats[ _op_idx ];

  _op_stats.count += 1;
  for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
  {
    _op_stats.cost[i] += tx_info.cost[i];
    _op_stats.usage[i] += tx_info.usage[i];
  }

  if( tx_info.max > 0 )
  {
    int _payer_rank = log10( tx_info.max ) - 9;
    if( _payer_rank < 0 )
      _payer_rank = 0;
    if( _payer_rank >= HIVE_RC_NUM_PAYER_RANKS )
      _payer_rank = HIVE_RC_NUM_PAYER_RANKS - 1;
    rc_payer_stats& _payer_stats = payer_stats[ _payer_rank ];

    _payer_stats.count += 1;
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
    {
      _payer_stats.cost[i] += tx_info.cost[i];
      _payer_stats.usage[i] += tx_info.usage[i];
    }
    // since it is just statistics we can do rough calculations:
    int64_t low_rc = tx_info.max / 20; // 5%
    if( tx_info.rc < low_rc )
      _payer_stats.less_than_5_percent += 1;
    low_rc <<= 1; // 10%
    if( tx_info.rc < low_rc )
      _payer_stats.less_than_10_percent += 1;
    low_rc <<= 1; // 20%
    if( tx_info.rc < low_rc )
      _payer_stats.less_than_20_percent += 1;

    for( int i = 0; i < 3; ++i )
      if( tx_info.rc < average_cost[i] )
        _payer_stats.cant_afford[i] += 1;
  }
}

} } // namespace hive::chain