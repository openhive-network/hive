#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/type.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/sst_file_reader.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <chainbase/state_snapshot_support.hpp>

#include <hive/chain/database.hpp>
#include <hive/plugins/state_snapshot/state_snapshot_manifest.hpp>

#include <fstream>

namespace bfs = boost::filesystem;

namespace hive
{
  namespace plugins
  {
    namespace state_snapshot
    {

      using ::rocksdb::DB;
      using ::rocksdb::DBOptions;
      using ::rocksdb::Options;
      using ::rocksdb::PinnableSlice;
      using ::rocksdb::ReadOptions;
      using ::rocksdb::Slice;

      /** Helper base class to cover all common functionality across defined comparators.
       *
       */
      class AComparator : public ::rocksdb::Comparator
      {
      public:
        virtual const char *Name() const override final
        {
          static const std::string name = boost::core::demangle(typeid(this).name());
          return name.c_str();
        }

        virtual void FindShortestSeparator(std::string *start, const Slice &limit) const override final
        {
          /// Nothing to do.
        }

        virtual void FindShortSuccessor(std::string *key) const override final
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
        virtual int Compare(const Slice &a, const Slice &b) const override
        {
          if (a.size() != sizeof(T) || b.size() != sizeof(T))
            return a.compare(b);

          const T &id1 = retrieveKey(a);
          const T &id2 = retrieveKey(b);

          if (id1 < id2)
            return -1;

          if (id1 > id2)
            return 1;

          return 0;
        }

        virtual bool Equal(const Slice &a, const Slice &b) const override
        {
          if (a.size() != sizeof(T) || b.size() != sizeof(T))
            return a == b;

          const auto &id1 = retrieveKey(a);
          const auto &id2 = retrieveKey(b);

          return id1 == id2;
        }

      private:
        const T &retrieveKey(const Slice &slice) const
        {
          assert(sizeof(T) == slice.size());
          const char *rawData = slice.data();
          return *reinterpret_cast<const T *>(rawData);
        }
      };

      typedef PrimitiveTypeComparatorImpl<size_t> size_t_comparator;
      extern size_t_comparator _size_t_comparator;

      template <class BaseClass>
      class snapshot_processor_data : public BaseClass
      {
      public:
        using typename BaseClass::snapshot_converter_t;

        const bfs::path &get_root_path() const
        {
          return _rootPath;
        }

        ::rocksdb::EnvOptions get_environment_config() const
        {
          return ::rocksdb::EnvOptions();
        }

        ::rocksdb::Options get_storage_config() const
        {
          ::rocksdb::Options opts;
          opts.comparator = &_size_t_comparator;

          opts.max_open_files = 1024;

          return opts;
        }

        snapshot_converter_t get_converter() const
        {
          return _converter;
        }

        void set_processing_success(bool s)
        {
          _processingSuccess = s;
        }

        bfs::path prepare_index_storage_path(const std::string &indexDescription) const
        {
          std::string baseFilePath = indexDescription;
          boost::replace_all(baseFilePath, " ", "_");
          boost::replace_all(baseFilePath, ":", "_");
          boost::replace_all(baseFilePath, "<", "_");
          boost::replace_all(baseFilePath, ">", "_");

          bfs::path storagePath(_rootPath);
          storagePath /= baseFilePath;

          return storagePath;
        }

        const std::string &getIndexDescription() const
        {
          return _indexDescription;
        }

        /// Returns true if given index shall be processed.
        bool process_index(const std::string &indexDescription) const
        {
          // if(indexDescription == "comment_object")
          return true;

          // return false;
        }

        bool perform_debug_logging() const
        {
          // if(indexDescription == "comment_object")
          //   return true;

          return false;
        }

      protected:
        snapshot_processor_data(const bfs::path &rootPath) : _rootPath(rootPath) {}

      protected:
        snapshot_converter_t _converter;
        bfs::path _rootPath;
        std::string _indexDescription;

        bool _processingSuccess = false;
      };

      class dumping_worker;
      class loading_worker;

      class index_dump_writer final : public snapshot_processor_data<chainbase::snapshot_writer>
      {
      public:
        index_dump_writer(const hive::chain::database &mainDb, const chainbase::abstract_index &index, const bfs::path &outputRootPath,
                          bool allow_concurrency, std::exception_ptr &exception, std::atomic_bool &is_error) : snapshot_processor_data<chainbase::snapshot_writer>(outputRootPath), _mainDb(mainDb), _index(index), _firstId(0), _lastId(0),
                                                                                                               _nextId(0), _allow_concurrency(allow_concurrency), _exception(exception), _is_error(is_error) {}

        index_dump_writer(const index_dump_writer &) = delete;
        index_dump_writer &operator=(const index_dump_writer &) = delete;

        virtual ~index_dump_writer() = default;

        virtual workers prepare(const std::string &indexDescription, size_t firstId, size_t lastId, size_t indexSize, size_t indexNextId,
                                snapshot_converter_t converter) override;
        virtual void start(const workers &workers) override;

        const hive::chain::database &getMainDb() const { return _mainDb; }

        void store_index_manifest(index_manifest_info *manifest) const;
        void safe_spawn_snapshot_dump(const chainbase::abstract_index *idx);

      private:
        const hive::chain::database &_mainDb;
        const chainbase::abstract_index &_index;
        std::vector<std::unique_ptr<dumping_worker>> _builtWorkers;
        size_t _firstId;
        size_t _lastId;
        size_t _nextId;
        bool _allow_concurrency;
        std::exception_ptr &_exception;
        std::atomic_bool &_is_error;
      };

      class index_dump_reader final : public snapshot_processor_data<chainbase::snapshot_reader>
      {
      public:
        index_dump_reader(const snapshot_manifest &snapshotManifest, const bfs::path &rootPath,
                          std::exception_ptr &exception, std::atomic_bool &is_error) : snapshot_processor_data<chainbase::snapshot_reader>(rootPath),
                                                                                       _snapshotManifest(snapshotManifest), currentWorker(nullptr), _exception(exception), _is_error(is_error) {}

        index_dump_reader(const index_dump_reader &) = delete;
        index_dump_reader &operator=(const index_dump_reader &) = delete;

        virtual ~index_dump_reader() = default;

        virtual workers prepare(const std::string &indexDescription, snapshot_converter_t converter, size_t *snapshot_index_next_id, size_t *snapshot_dumped_items) override;
        virtual void start(const workers &workers) override;

        size_t getCurrentlyProcessedId() const;
        void safe_spawn_snapshot_load(chainbase::abstract_index *idx);

      private:
        const snapshot_manifest &_snapshotManifest;
        std::vector<std::unique_ptr<loading_worker>> _builtWorkers;
        const loading_worker *currentWorker;
        std::exception_ptr &_exception;
        std::atomic_bool &_is_error;
      };

      class dumping_worker final : public chainbase::snapshot_writer::worker
      {
      public:
        dumping_worker(const bfs::path &outputFile, index_dump_writer &writer, size_t startId, size_t endId, const std::atomic_bool &is_error) : chainbase::snapshot_writer::worker(writer, startId, endId), _controller(writer), _outputFile(outputFile),
                                                                                                                                                 _writtenEntries(0), _write_finished(false), _is_error(is_error) {}

        virtual ~dumping_worker()
        {
          if (_writer)
            _writer->Finish();
        }

        void perform_dump();

        void store_index_manifest(index_manifest_info *manifest) const;

        bool is_write_finished(size_t *writtenEntries = nullptr) const
        {
          if (writtenEntries != nullptr)
            *writtenEntries = _writtenEntries;

          return _write_finished;
        }

        const bfs::path &get_output_file() const
        {
          return _outputFile;
        }

      private:
        virtual void flush_converted_data(const serialized_object_cache &cache) override;
        virtual std::string prettifyObject(const fc::variant &object, const std::vector<char> &buffer) const override
        {
          std::string s;

          if (_controller.perform_debug_logging())
          {
            s = fc::json::to_pretty_string(object);
            s += '\n';
            s += "Buffer size: " + std::to_string(buffer.size());
          }

          return s;
        }

        void prepareWriter();

      private:
        index_dump_writer &_controller;
        bfs::path _outputFile;
        std::unique_ptr<::rocksdb::SstFileWriter> _writer;
        ::rocksdb::ExternalSstFileInfo _sstFileInfo;
        size_t _writtenEntries;
        bool _write_finished;
        const std::atomic_bool &_is_error;
      };

      class loading_worker final : public chainbase::snapshot_reader::worker
      {
      public:
        loading_worker(const index_manifest_info &manifestInfo, const bfs::path &inputPath, index_dump_reader &reader, const std::atomic_bool &is_error) : chainbase::snapshot_reader::worker(reader, 0, 0),
                                                                                                                                                           _manifestInfo(manifestInfo), _controller(reader), _inputPath(inputPath), _is_error(is_error)
        {
          _startId = manifestInfo.firstId;
          _endId = manifestInfo.lastId;
        }

        virtual ~loading_worker() = default;

        virtual void load_converted_data(worker_common_base::serialized_object_cache *cache) override;
        virtual std::string prettifyObject(const fc::variant &object, const std::vector<char> &buffer) const override
        {
          std::string s;
          if (_controller.perform_debug_logging())
          {
            s = fc::json::to_pretty_string(object);
            s += '\n';
            s += "Data buffer size: " + std::to_string(buffer.size());
          }

          return s;
        }

        void perform_load();

      private:
        const index_manifest_info &_manifestInfo;
        index_dump_reader &_controller;
        bfs::path _inputPath;
        std::unique_ptr<::rocksdb::SstFileReader> _reader;
        std::unique_ptr<::rocksdb::Iterator> _entryIt;
        const std::atomic_bool &_is_error;
      };

      class dumping_to_file_worker;

      class index_dump_to_file_writer : public chainbase::snapshot_writer
      {
      public:
        index_dump_to_file_writer(const index_dump_to_file_writer &) = delete;
        index_dump_to_file_writer &operator=(const index_dump_to_file_writer &) = delete;
        virtual ~index_dump_to_file_writer() = default;

        index_dump_to_file_writer(const chainbase::database &_db, const chainbase::abstract_index &_index, const fc::path &_output_dir, bool _allow_concurrency)
            : database(_db), index(_index), output_dir(_output_dir), allow_concurrency(_allow_concurrency) { conversion_type = chainbase::snapshot_writer::conversion_t::convert_to_json; }

        virtual workers prepare(const std::string &indexDescription, size_t firstId, size_t lastId, size_t indexSize, size_t indexNextId, snapshot_converter_t converter) override;
        virtual void start(const workers &workers) override;

        snapshot_converter_t get_snapshot_converter() const { return snapshot_converter; }

      private:
        std::vector<std::unique_ptr<dumping_to_file_worker>> snapshot_writer_workers;
        const chainbase::database &database;
        const chainbase::abstract_index &index;
        const fc::path output_dir;
        snapshot_converter_t snapshot_converter;
        std::string index_description;
        size_t first_id;
        size_t last_id;
        size_t next_id;
        bool allow_concurrency;
      };

      class dumping_to_file_worker : public chainbase::snapshot_writer::worker
      {
      public:
        dumping_to_file_worker(const fc::path &_output_file, index_dump_to_file_writer &_writer, const size_t _start_id, const size_t _end_id);
        void perform_dump();
        bool is_write_finished() const { return write_finished; }

      private:
        virtual void flush_converted_data(const serialized_object_cache &cache) override;
        virtual std::string prettifyObject(const fc::variant &object, const std::vector<char> &buffer) const override { return ""; }

        index_dump_to_file_writer &controller;
        fc::path output_file_path;
        std::fstream output_file;
        bool write_finished = false;
      };

    }
  }
} // hive::plugins::state_snapshot
