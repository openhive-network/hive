#pragma once

#include <hive/chain/sync_block_writer.hpp>

namespace consensus_state_provider
{

  class csp_session_type;
  using csp_session_ref_type = csp_session_type&;
  using csp_session_ptr_type = csp_session_type*;


  class custom_block_reader : public hive::chain::fork_db_block_reader 
  {
  public:
    custom_block_reader(hive::chain::fork_database& fork_db, hive::chain::block_log& block_log, csp_session_ref_type csp_session)
      : fork_db_block_reader(fork_db, block_log), csp_session(csp_session) 
    {
    }

    virtual std::shared_ptr<hive::chain::full_block_type> read_block_by_num( uint32_t block_num ) const override;
    private:
      csp_session_ref_type csp_session;
  
  };

  class empty_block_writer : public hive::chain::sync_block_writer
  {
      virtual void store_block( uint32_t current_irreversible_block_num, uint32_t state_head_block_number ) override
      {

      }

    public:
      empty_block_writer( hive::chain::database& db, appbase::application& app, csp_session_ref_type csp_session )
      :
        sync_block_writer(db, app), _custom_reader(_fork_db, _block_log, csp_session) 
      {
      }
    virtual hive::chain::block_read_i& get_block_reader() override {
      return _custom_reader;
    }

private:
  custom_block_reader _custom_reader;

  };

  
  } // namespace consensus_state_provider

