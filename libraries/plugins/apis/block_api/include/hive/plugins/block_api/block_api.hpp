#pragma once

#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/plugins/block_api/block_api_args.hpp>

#define BLOCK_API_SINGLE_QUERY_LIMIT 1000

namespace hive { namespace plugins { namespace block_api {

class block_api_impl;

class block_api
{
  public:
    block_api();
    ~block_api();

    DECLARE_API(

      /////////////////////////////
      // Blocks and transactions //
      /////////////////////////////

      /**
      * @brief Retrieve a block header
      * @param block_num Height of the block whose header should be returned
      * @return header of the referenced block, or null if no matching block was found
      */
      (get_block_header)

      /**
      * @brief Retrieve a full, signed block
      * @param block_num Height of the block to be returned
      * @return the referenced block, or null if no matching block was found
      */
      (get_block)

      /**
      * @brief Retrieve a range of full, signed blocks
      * @param starting_block_num Height of the first block to be returned
      * @param count the maximum number of blocks to return
      * @return the blocks requested.  The list may be shorter than requested if `count` blocks
      *         would take you past the current head block
      */
      (get_block_range)
    )

  private:
    std::unique_ptr< block_api_impl > my;
};

} } } //hive::plugins::block_api

