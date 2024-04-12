#pragma once
#include <hive/chain/hive_object_types.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace hive { namespace plugins { namespace follow {

using namespace std;
using namespace hive::chain;

using chainbase::t_vector;

#ifndef HIVE_FOLLOW_SPACE_ID
#define HIVE_FOLLOW_SPACE_ID 8
#endif

enum follow_plugin_object_type
{
  follow_object_type            = ( HIVE_FOLLOW_SPACE_ID << 8 ),
  feed_object_type              = ( HIVE_FOLLOW_SPACE_ID << 8 ) + 1,
  reputation_object_type        = ( HIVE_FOLLOW_SPACE_ID << 8 ) + 2,
  blog_object_type              = ( HIVE_FOLLOW_SPACE_ID << 8 ) + 3,
  follow_count_object_type      = ( HIVE_FOLLOW_SPACE_ID << 8 ) + 4,
  blog_author_stats_object_type = ( HIVE_FOLLOW_SPACE_ID << 8 ) + 5
};

enum follow_type
{
  undefined,
  blog,
  ignore
};

class follow_object : public object< follow_object_type, follow_object >
{
  CHAINBASE_OBJECT( follow_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( follow_object )

    account_name_type follower;
    account_name_type following;
    uint16_t          what = 0;
};

typedef oid_ref< follow_object > follow_id_type;


class feed_object : public object< feed_object_type, feed_object, std::true_type >
{
  CHAINBASE_OBJECT( feed_object );
  public:
    typedef t_vector<account_name_type> t_reblogged_by_container;

    CHAINBASE_DEFAULT_CONSTRUCTOR( feed_object, (reblogged_by) )

    account_name_type          account;
    t_reblogged_by_container   reblogged_by;
    account_name_type          first_reblogged_by;
    time_point_sec             first_reblogged_on;
    comment_id_type            comment;
    uint32_t                   account_feed_id = 0;
};

typedef oid_ref< feed_object > feed_id_type;


class blog_object : public object< blog_object_type, blog_object >
{
  CHAINBASE_OBJECT( blog_object, true );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( blog_object )

    account_name_type account;
    comment_id_type   comment;
    time_point_sec    reblogged_on;
    uint32_t          blog_feed_id = 0;
};
typedef oid_ref< blog_object > blog_id_type;

/**
  *  This index is maintained to get an idea of which authors are rehived by a particular blogger and
  *  how frequnetly. It is designed to give an overview of the type of people a blogger sponsors as well
  *  as to enable generation of filter set for a blog list.
  *
  *  Give me the top authors promoted by this blog
  *  Give me all blog posts by [authors] that were rehived by this blog
  */
class blog_author_stats_object : public object< blog_author_stats_object_type, blog_author_stats_object >
{
  CHAINBASE_OBJECT( blog_author_stats_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( blog_author_stats_object )

    account_name_type blogger;
    account_name_type guest;
    uint32_t          count = 0;
};

typedef oid_ref< blog_author_stats_object > blog_author_stats_id_type;



class reputation_object : public object< reputation_object_type, reputation_object >
{
  CHAINBASE_OBJECT( reputation_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( reputation_object )

    account_name_type account;
    share_type        reputation;
};

typedef oid_ref< reputation_object > reputation_id_type;


class follow_count_object : public object< follow_count_object_type, follow_count_object >
{
  CHAINBASE_OBJECT( follow_count_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( follow_count_object )

    account_name_type account;
    uint32_t          follower_count  = 0;
    uint32_t          following_count = 0;
};

typedef oid_ref< follow_count_object > follow_count_id_type;


struct by_following_follower;
struct by_follower_following;

typedef multi_index_container<
  follow_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< follow_object, follow_object::id_type, &follow_object::get_id > >,
    ordered_unique< tag< by_following_follower >,
      composite_key< follow_object,
        member< follow_object, account_name_type, &follow_object::following >,
        member< follow_object, account_name_type, &follow_object::follower >
      >,
      composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
    >,
    ordered_unique< tag< by_follower_following >,
      composite_key< follow_object,
        member< follow_object, account_name_type, &follow_object::follower >,
        member< follow_object, account_name_type, &follow_object::following >
      >,
      composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
    >
  >,
  allocator< follow_object >
> follow_index;

struct by_blogger_guest_count;
typedef multi_index_container<
  blog_author_stats_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< blog_author_stats_object, blog_author_stats_object::id_type, &blog_author_stats_object::get_id > >,
    ordered_unique< tag< by_blogger_guest_count >,
      composite_key< blog_author_stats_object,
        member< blog_author_stats_object, account_name_type, &blog_author_stats_object::blogger >,
        member< blog_author_stats_object, account_name_type, &blog_author_stats_object::guest >,
        member< blog_author_stats_object, uint32_t, &blog_author_stats_object::count >
      >,
      composite_key_compare< std::less< account_name_type >, std::less< account_name_type >, greater<uint32_t> >
    >
  >,
  allocator< blog_author_stats_object >
> blog_author_stats_index;

struct by_feed;
struct by_account;
struct by_comment;

typedef multi_index_container<
  feed_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< feed_object, feed_object::id_type, &feed_object::get_id > >,
    ordered_unique< tag< by_feed >,
      composite_key< feed_object,
        member< feed_object, account_name_type, &feed_object::account >,
        member< feed_object, uint32_t, &feed_object::account_feed_id >
      >,
      composite_key_compare< std::less< account_name_type >, std::greater< uint32_t > >
    >,
    ordered_unique< tag< by_comment >,
      composite_key< feed_object,
        member< feed_object, comment_id_type, &feed_object::comment >,
        member< feed_object, account_name_type, &feed_object::account >,
        const_mem_fun< feed_object, feed_object::id_type, &feed_object::get_id >
      >,
      composite_key_compare< std::less< comment_id_type >, std::less< account_name_type >, std::less< feed_id_type > >
    >
  >,
  allocator< feed_object >
> feed_index;

struct by_blog;

typedef multi_index_container<
  blog_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< blog_object, blog_object::id_type, &blog_object::get_id > >,
    ordered_unique< tag< by_blog >,
      composite_key< blog_object,
        member< blog_object, account_name_type, &blog_object::account >,
        member< blog_object, uint32_t, &blog_object::blog_feed_id >
      >,
      composite_key_compare< std::less< account_name_type >, std::greater< uint32_t > >
    >,
    ordered_unique< tag< by_comment >,
      composite_key< blog_object,
        member< blog_object, comment_id_type, &blog_object::comment >,
        member< blog_object, account_name_type, &blog_object::account >,
        const_mem_fun< blog_object, blog_object::id_type, &blog_object::get_id >
      >,
      composite_key_compare< std::less< comment_id_type >, std::less< account_name_type >, std::less< blog_id_type > >
    >
  >,
  allocator< blog_object >
> blog_index;

typedef multi_index_container<
  reputation_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< reputation_object, reputation_object::id_type, &reputation_object::get_id > >,
    ordered_unique< tag< by_account >,
      member< reputation_object, account_name_type, &reputation_object::account > >
  >,
  allocator< reputation_object >
> reputation_index;


struct by_followers;
struct by_following;

typedef multi_index_container<
  follow_count_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< follow_count_object, follow_count_object::id_type, &follow_count_object::get_id > >,
    ordered_unique< tag< by_account >,
      member< follow_count_object, account_name_type, &follow_count_object::account > >
  >,
  allocator< follow_count_object >
> follow_count_index;

} } } // hive::plugins::follow

FC_REFLECT_ENUM( hive::plugins::follow::follow_type, (undefined)(blog)(ignore) )

FC_REFLECT( hive::plugins::follow::follow_object, (id)(follower)(following)(what) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::follow::follow_object, hive::plugins::follow::follow_index )

FC_REFLECT( hive::plugins::follow::feed_object, (id)(account)(first_reblogged_by)(first_reblogged_on)(reblogged_by)(comment)(account_feed_id) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::follow::feed_object, hive::plugins::follow::feed_index )

FC_REFLECT( hive::plugins::follow::blog_object, (id)(account)(comment)(reblogged_on)(blog_feed_id) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::follow::blog_object, hive::plugins::follow::blog_index )

FC_REFLECT( hive::plugins::follow::reputation_object, (id)(account)(reputation) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::follow::reputation_object, hive::plugins::follow::reputation_index )

FC_REFLECT( hive::plugins::follow::follow_count_object, (id)(account)(follower_count)(following_count) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::follow::follow_count_object, hive::plugins::follow::follow_count_index )

FC_REFLECT( hive::plugins::follow::blog_author_stats_object, (id)(blogger)(guest)(count) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::follow::blog_author_stats_object, hive::plugins::follow::blog_author_stats_index );

namespace helpers
{
  template <>
  class index_statistic_provider<hive::plugins::follow::feed_index>
  {
  public:
    typedef hive::plugins::follow::feed_index IndexType;
    typedef typename hive::plugins::follow::feed_object::t_reblogged_by_container t_reblogged_by_container;

    size_t get_item_additional_allocation(const hive::plugins::follow::feed_object& o) const
    {
      size_t size = 0;
      size += o.reblogged_by.capacity()*sizeof(t_reblogged_by_container::value_type);
      return size;
    }

    index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
    {
      index_statistic_info info;
      gather_index_static_data(index, &info);

      if(onlyStaticInfo == false)
      {
        for(const auto& o : index)
        {
          info._item_additional_allocation += get_item_additional_allocation(o);
        }
      }

      return info;
    }
  };

} /// namespace helpers
