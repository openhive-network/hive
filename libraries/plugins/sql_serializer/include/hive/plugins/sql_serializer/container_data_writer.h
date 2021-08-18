#pragma once

#include <hive/plugins/sql_serializer/trigger_synchronous_masive_sync_call.hpp>
#include <hive/plugins/sql_serializer/data_processor.hpp>

#include <fc/exception/exception.hpp>

namespace hive::plugins::sql_serializer {
  /**
   * @brief Common implementation of data writer to be used for all SQL entities.
   *
   * @tparam DataContainer temporary container providing a data chunk.
   * @tparam TupleConverter a functor to convert volatile representation (held in the DataContainer) into SQL representation
   *                        TupleConverter must match to function interface:
   *                        std::string(pqxx::work& tx, typename DataContainer::const_reference)
   *
  */
  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
    class container_data_writer
      {
      public:
        container_data_writer(std::string psqlUrl, std::string description, std::shared_ptr< trigger_synchronous_masive_sync_call > _api_trigger );

        void trigger(DataContainer&& data, bool wait_for_data_completion, uint32_t last_block_num);
        void complete_data_processing();
        void join();

      private:
        using data_processing_status = data_processor::data_processing_status;
        using data_chunk_ptr = data_processor::data_chunk_ptr;

        static data_processing_status flush_replayed_data(const data_chunk_ptr& dataPtr, transaction& tx);
        static data_processing_status flush_array_live_data(const data_chunk_ptr& dataPtr, transaction& tx);
        static data_processing_status flush_scalar_live_data(const data_chunk_ptr& dataPtr, transaction& tx);


      private:
        class chunk : public data_processor::data_chunk
          {
          public:
            chunk( DataContainer&& data) : _data(std::move(data)) {}
            ~chunk() = default;

            DataContainer _data;
          };

      private:
        std::unique_ptr<queries_commit_data_processor> _processor;
      };

  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
  inline
  container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::container_data_writer(std::string psqlUrl, std::string description, std::shared_ptr< trigger_synchronous_masive_sync_call > _api_trigger )
  {
    _processor = std::make_unique<queries_commit_data_processor>(psqlUrl, description, flush_replayed_data, _api_trigger);
  }

  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
  inline void
  container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::trigger(DataContainer&& data, bool wait_for_data_completion, uint32_t last_block_num)
  {
    if(data.empty() == false)
    {
      _processor->trigger(std::make_unique<chunk>(std::move(data)), last_block_num);
      if(wait_for_data_completion)
        _processor->complete_data_processing();
    } else {
      _processor->only_report_batch_finished( last_block_num );
    }

    FC_ASSERT(data.empty());
  }

  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
  inline void
  container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::complete_data_processing()
  {
    _processor->complete_data_processing();
  }

  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
  inline void
  container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::join()
  {
    _processor->join();
  }

  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
  inline typename container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::data_processing_status
  container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::flush_replayed_data(const data_chunk_ptr& dataPtr, transaction& tx)
  {
    const chunk* holder = static_cast<const chunk*>(dataPtr.get());
    data_processing_status processingStatus;

    TupleConverter conv;

    const DataContainer& data = holder->_data;

    FC_ASSERT(data.empty() == false);

    std::string query = "INSERT INTO ";
    query += TABLE_NAME;
    query += '(';
    query += COLUMN_LIST;
    query += ") VALUES\n";

    auto dataI = data.cbegin();
    query += '(' + conv(*dataI) + ")\n";

    for(++dataI; dataI != data.cend(); ++dataI)
    {
      query += ",(" + conv(*dataI) + ")\n";
    }

    query += ';';

    tx.exec(query);

    processingStatus.first += data.size();
    processingStatus.second = true;

    return processingStatus;

  }

  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
  inline typename container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::data_processing_status
  container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::flush_array_live_data(const data_chunk_ptr& dataPtr, transaction& tx)
  {
    const chunk* holder = static_cast<const chunk*>(dataPtr.get());
    data_processing_status processingStatus;

    TupleConverter conv;

    const DataContainer& data = holder->_data;

    FC_ASSERT(data.empty() == false);

    std::string query = "ARRAY[";

    auto dataI = data.cbegin();
    query += '(' + conv(*dataI) + ")\n";

    for(++dataI; dataI != data.cend(); ++dataI)
    {
      query += ",(" + conv(*dataI) + ")\n";
    }

    query += ']';

    processingStatus.first += data.size();
    processingStatus.second = true;

    return processingStatus;

  }

  template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
  inline typename container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::data_processing_status
  container_data_writer<DataContainer, TupleConverter, TABLE_NAME, COLUMN_LIST >::flush_scalar_live_data(const data_chunk_ptr& dataPtr, transaction& tx)
  {
    const chunk* holder = static_cast<const chunk*>(dataPtr.get());
    data_processing_status processingStatus;

    TupleConverter conv;

    const DataContainer& data = holder->_data;

    FC_ASSERT(data.empty() == false);

    std::string query = "";

    auto dataI = data.cbegin();
    query += '(' + conv(*dataI) + ")\n";

    for(++dataI; dataI != data.cend(); ++dataI)
    {
      query += ",(" + conv(*dataI) + ")\n";
    }

    processingStatus.first += data.size();
    processingStatus.second = true;

    return processingStatus;
  }

} // namespace hive::plugins::sql_serializer