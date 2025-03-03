#pragma once

#include "fc/exception/exception.hpp"
#include <chainbase/dumper_custom_data.hpp>

#include <hive/chain/comment_object.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/sst_file_reader.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <boost/filesystem.hpp>

namespace hive {

using ::rocksdb::DB;
using ::rocksdb::DBOptions;
using ::rocksdb::Options;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ReadOptions;
using ::rocksdb::Slice;

class AComparator : public ::rocksdb::Comparator
  {
  public:
    virtual const char* Name() const override final
      {
      static const std::string name = boost::core::demangle(typeid(this).name());
      return name.c_str();
      }

    virtual void FindShortestSeparator(std::string* start, const Slice& limit) const override final
      {
        /// Nothing to do.
      }

    virtual void FindShortSuccessor(std::string* key) const override final
      {
        /// Nothing to do.
      }

  protected:
    AComparator() = default;
  };

  template <typename T>
  class PrimitiveTypeComparatorImpl final : public AComparator
    {
    public:
      virtual int Compare(const Slice& a, const Slice& b) const override
        {
        if(a.size() != sizeof(T) || b.size() != sizeof(T))
          return a.compare(b);

        const T& id1 = retrieveKey(a);
        const T& id2 = retrieveKey(b);

        if(id1 < id2)
          return -1;

        if(id1 > id2)
          return 1;

        return 0;
        }

      virtual bool Equal(const Slice& a, const Slice& b) const override
        {
        if(a.size() != sizeof(T) || b.size() != sizeof(T))
          return a == b;

        const auto& id1 = retrieveKey(a);
        const auto& id2 = retrieveKey(b);

        return id1 == id2;
        }

    private:
      const T& retrieveKey(const Slice& slice) const
        {
        assert(sizeof(T) == slice.size());
        const char* rawData = slice.data();
        return *reinterpret_cast<const T*>(rawData);
        }
    };

  typedef PrimitiveTypeComparatorImpl<std::string> string_comparator;

  namespace bfs = boost::filesystem;
  class dumper_comment_data: public chainbase::dumper_custom_data
  {
    private:

      string_comparator _string_comparator;

      void write_impl( const std::string& obj );

    public:

      bfs::path _storagePath;
      bfs::path _outputFile;

      std::unique_ptr<::rocksdb::SstFileWriter> _writer;
      //::rocksdb::ExternalSstFileInfo _sstFileInfo;

      dumper_comment_data();

      ~dumper_comment_data();

      ::rocksdb::EnvOptions get_environment_config() const;

      ::rocksdb::Options get_storage_config() const;

      void prepare( const std::string& snapshotName );

      void prepareWriter();

      void write( const void* obj ) override;
  };
}