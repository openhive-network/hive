#pragma once

#include <hive/plugins/sql_serializer/container_data_writer.h>
#include <hive/plugins/sql_serializer/data_processor.hpp>

namespace hive::plugins::sql_serializer {
  template <typename TableDescriptor, typename Processor = queries_commit_data_processor>
  using table_data_writer = container_data_writer<
      typename TableDescriptor::container_t
    , typename TableDescriptor::data2sql_tuple
    , TableDescriptor::TABLE, TableDescriptor::COLS
    , Processor
  >;

} // namespace hive::plugins::sql_serializer
