#pragma once
#include <chainbase/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#include <hive/chain/comment_object.hpp>


namespace hive { namespace plugins { namespace tags {

using namespace hive::chain;

using namespace appbase;

using chainbase::object;
using chainbase::oid;
using chainbase::oid_ref;
using chainbase::allocator;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef HIVE_TAG_SPACE_ID
#define HIVE_TAG_SPACE_ID 5
#endif

#define HIVE_TAGS_PLUGIN_NAME "tags"

typedef protocol::fixed_string< 32 > tag_name_type;

// Plugins need to define object type IDs such that they do not conflict
// globally. If each plugin uses the upper 8 bits as a space identifier,
// with 0 being for chain, then the lower 8 bits are free for each plugin
// to define as they see fit.
enum
{
  tag_object_type              = ( HIVE_TAG_SPACE_ID << 8 ),
  tag_stats_object_type        = ( HIVE_TAG_SPACE_ID << 8 ) + 1,
  peer_stats_object_type       = ( HIVE_TAG_SPACE_ID << 8 ) + 2,
  author_tag_stats_object_type = ( HIVE_TAG_SPACE_ID << 8 ) + 3
};

namespace detail { class tags_plugin_impl; }


/**
  *  The purpose of the tag object is to allow the generation and listing of
  *  all top level posts by a string tag.  The desired sort orders include:
  *
  *  1. created - time of creation
  *  2. maturing - about to receive a payout
  *  3. active - last reply the post or any child of the post
  *  4. netvotes - individual accounts voting for post minus accounts voting against it
  *
  *  When ever a comment is modified, all tag_objects for that comment are updated to match.
  */
class tag_object : public object< tag_object_type, tag_object >
{
  CHAINBASE_OBJECT( tag_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( tag_object )

    tag_name_type     tag;
    time_point_sec    created;
    time_point_sec    active;
    time_point_sec    cashout;
    int64_t           net_rshares = 0;
    int32_t           net_votes   = 0;
    int32_t           children    = 0;
    double            hot         = 0;
    double            trending    = 0;
    share_type        promoted_balance = 0;

    account_id_type   author;
    comment_id_type   parent;
    comment_id_type   comment;

    bool is_post()const { return parent == comment_object::id_type::null_id(); }
};

typedef oid_ref< tag_object > tag_id_type;

/**
  *  The purpose of this index is to quickly identify how popular various tags by maintaining variou sums over
  *  all posts under a particular tag
  */
class tag_stats_object : public object< tag_stats_object_type, tag_stats_object >
{
  CHAINBASE_OBJECT( tag_stats_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( tag_stats_object )

    tag_name_type     tag;
    asset             total_payout = asset( 0, HBD_SYMBOL );
    int32_t           net_votes = 0;
    uint32_t          top_posts = 0;
    uint32_t          comments  = 0;
    fc::uint128       total_trending = 0;
};

typedef oid_ref< tag_stats_object > tag_stats_id_type;

struct by_comments;
struct by_top_posts;
struct by_trending;

/**
  *  This purpose of this object is to maintain stats about which tags an author uses, how frequnetly, and
  *  how many total earnings of all posts by author in tag.  It also allows us to answer the question of which
  *  authors earn the most in each tag category.  This helps users to discover the best bloggers to follow for
  *  particular tags.
  */
class author_tag_stats_object : public object< author_tag_stats_object_type, author_tag_stats_object >
{
  CHAINBASE_OBJECT( author_tag_stats_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( author_tag_stats_object )

    account_id_type author;
    tag_name_type   tag;
    asset           total_rewards = asset( 0, HBD_SYMBOL );
    uint32_t        total_posts = 0;
};
typedef oid_ref< author_tag_stats_object > author_tag_stats_id_type;


/**
  * Used to parse the metadata from the comment json_meta field.
  */
struct comment_metadata { set<string> tags; };

/**
  *  This plugin will scan all changes to posts and/or their meta data and
  *
  */
class tags_plugin : public plugin< tags_plugin >
{
  public:
    tags_plugin();
    virtual ~tags_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_TAGS_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    friend class detail::tags_plugin_impl;

  private:
    std::unique_ptr< detail::tags_plugin_impl > my;
};

} } } //hive::plugins::tags

FC_REFLECT( hive::plugins::tags::tag_object,
  (id)(tag)(created)(active)(cashout)(net_rshares)(net_votes)(hot)(trending)(promoted_balance)(children)(author)(parent)(comment) )


FC_REFLECT( hive::plugins::tags::tag_stats_object,
  (id)(tag)(total_payout)(net_votes)(top_posts)(comments)(total_trending) );

FC_REFLECT( hive::plugins::tags::comment_metadata, (tags) );

FC_REFLECT( hive::plugins::tags::author_tag_stats_object, (id)(author)(tag)(total_posts)(total_rewards) )
