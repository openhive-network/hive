#pragma once

#include <hive/plugins/sql_serializer/container_data_writer.h>

namespace hive::plugins::sql_serializer {
  template <typename TableDescriptor>
  using table_data_writer = container_data_writer<
      typename TableDescriptor::container_t
    , typename TableDescriptor::data2sql_tuple
    , TableDescriptor::TABLE, TableDescriptor::COLS
  >;

} // namespace hive::plugins::sql_serializer
