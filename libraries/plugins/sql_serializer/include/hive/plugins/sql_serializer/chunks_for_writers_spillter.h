#pragma once

#include <cmath>
#include <memory>
#include <vector>

namespace hive::plugins::sql_serializer {
  class block_num_rendezvous_trigger;

  template< typename TableWriter >
  class chunks_for_writers_splitter
    {
    public:
      chunks_for_writers_splitter( uint8_t number_of_writers,
                                     std::string psqlUrl
                                   , std::string description
                                   , std::shared_ptr< block_num_rendezvous_trigger > _api_trigger
                                   );
      ~chunks_for_writers_splitter() = default;

      chunks_for_writers_splitter( chunks_for_writers_splitter& ) = delete;
      chunks_for_writers_splitter( chunks_for_writers_splitter&& ) = delete;
      chunks_for_writers_splitter& operator=( chunks_for_writers_splitter& ) = delete;
      chunks_for_writers_splitter& operator=( chunks_for_writers_splitter&& ) = delete;

      void trigger( typename TableWriter::DataContainerType&& data, uint32_t last_block_num );
      void join();
      void complete_data_processing();
    private:
      std::vector< TableWriter > writers;
      const std::string _description;
    };

  template< typename TableWriter >
  inline
  chunks_for_writers_splitter< TableWriter >::chunks_for_writers_splitter(
        uint8_t number_of_writers
      , std::string psqlUrl
      , std::string description
      , std::shared_ptr< block_num_rendezvous_trigger > _api_trigger
  )
  : _description( std::move(description) )
  {
    FC_ASSERT( number_of_writers > 0 );
    for ( auto writer_num = 0; writer_num < number_of_writers; ++writer_num ) {
      auto writer_description = _description + "_" + std::to_string( writer_num );
      writers.emplace_back( psqlUrl, writer_description, _api_trigger );
    }
  }

  template< typename TableWriter >
  inline void
  chunks_for_writers_splitter< TableWriter >::trigger( typename TableWriter::DataContainerType&& data, uint32_t last_block_num ) {
    uint32_t length_of_batch = std::ceil( data.size() / double( writers.size() ) );
    uint32_t writer_num = 0;
    for ( auto& writer : writers ) {
      typename TableWriter::DataContainerType batch(
          data.begin() + writer_num * length_of_batch
          ,  ( ( ( writer_num + 1 ) * length_of_batch ) < data.size() )
             ? data.begin() + ( writer_num + 1 ) * length_of_batch
             : data.end()
      );
      writer.trigger( std::move(batch), false, last_block_num );
      ++writer_num;
    }
    data.clear();
  }

  template< typename TableWriter >
  inline void
  chunks_for_writers_splitter< TableWriter >::join() {
    for ( auto& writer : writers ) {
      writer.join();
    }
  }

  template< typename TableWriter >
  inline void
  chunks_for_writers_splitter< TableWriter >::complete_data_processing() {
    for ( auto& writer : writers ) {
      writer.complete_data_processing();
    }
  }

} //namespace hive::plugins::sql_serializer
