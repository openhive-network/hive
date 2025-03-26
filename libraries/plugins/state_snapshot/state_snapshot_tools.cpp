#include <hive/plugins/state_snapshot/state_snapshot_tools.hpp>

#include <boost/asio/io_service.hpp>

#define ITEMS_PER_WORKER ((size_t)2000000)

namespace hive
{
  namespace plugins
  {
    namespace state_snapshot
    {

      size_t_comparator _size_t_comparator;

      void dumping_worker::perform_dump()
      {
        ilog("Performing a dump into file ${p}", ("p", _outputFile.string()));

        try
        {
          auto converter = _controller.get_converter();
          converter(this);

          if (_writer)
          {
            _writer->Finish(&_sstFileInfo);
            _writer.release();
          }

          _write_finished = true;

          ilog("Finished dump into file ${p}", ("p", _outputFile.string()));
        }
        FC_CAPTURE_AND_LOG(())
      }

      void dumping_worker::store_index_manifest(index_manifest_info *manifest) const
      {
        FC_ASSERT(is_write_finished(), "Write not finished yet for worker processing range: <${b}, ${e}> for index: `${i}'",
                  ("b", _startId)("e", _endId)("i", _controller.getIndexDescription()));
        FC_ASSERT(!_writer);

        FC_ASSERT(_writtenEntries == 0 || _sstFileInfo.file_size != 0);

        auto relativePath = bfs::relative(_outputFile, _controller.get_root_path());

        manifest->storage_files.emplace_back(hive::plugins::state_snapshot::index_manifest_file_info{relativePath.string(), _sstFileInfo.file_size});
      }

      void dumping_worker::flush_converted_data(const serialized_object_cache &cache)
      {
        if (!_writer)
          prepareWriter();

        ilog("Flushing converted data <${f}:${b}> for file ${o}", ("f", cache.front().first)("b", cache.back().first)("o", _outputFile.string()));

        for (const auto &kv : cache)
        {

          if (BOOST_UNLIKELY(_is_error.load()))
          {
            FC_ASSERT(false, "During a snapshot writing an error is detected. Writing is stopped.");
          }

          Slice key(reinterpret_cast<const char *>(&kv.first), sizeof(kv.first));
          Slice value(kv.second.data(), kv.second.size());
          auto status = _writer->Put(key, value);
          if (status.ok() == false)
          {
            elog("Cannot write to output file: `${p}'. Error details: `${e}'.", ("p", _outputFile.string())("e", status.ToString()));
            ilog("Failing key value: ${k}", ("k", kv.first));

            throw std::exception();
          }
        }

        _writtenEntries += cache.size();
      }

      void dumping_worker::prepareWriter()
      {
        auto options = _controller.get_storage_config();
        auto envOptions = _controller.get_environment_config();

        _writer = std::make_unique<::rocksdb::SstFileWriter>(envOptions, options);
        ilog("Trying to open SST Writer output file: `${p}'", ("p", _outputFile.string()));

        auto status = _writer->Open(_outputFile.string());
        if (status.ok())
        {
          ilog("Successfully opened SST Writer output file: `${p}'", ("p", _outputFile.string()));
        }
        else
        {
          elog("Cannot open SST Writer output file: `${p}'. Error details: `${e}'.", ("p", _outputFile.string())("e", status.ToString()));
          _writer.release();
          throw std::exception();
        }
      }

      chainbase::snapshot_writer::workers
      index_dump_writer::prepare(const std::string &indexDescription, size_t firstId, size_t lastId, size_t indexSize,
                                 size_t indexNextId, snapshot_converter_t converter)
      {
        ilog("Preparing snapshot writer to store index holding `${d}' items. Index size: ${s}. Index next_id: ${indexNextId}.Index id range: <${f}, ${l}>.",
             ("d", indexDescription)("s", indexSize)(indexNextId)("f", firstId)("l", lastId));

        _converter = converter;
        _indexDescription = indexDescription;
        _firstId = firstId;
        _lastId = lastId;
        _nextId = indexNextId;

        if (indexSize == 0 || process_index(indexDescription) == false)
          return workers();

        chainbase::snapshot_writer::workers retVal;

        size_t workerCount = indexSize / ITEMS_PER_WORKER + 1;
        size_t left = firstId;
        size_t right = 0;

        if (indexSize <= ITEMS_PER_WORKER)
          right = lastId;
        else
          right = std::min(firstId + ITEMS_PER_WORKER, lastId);

        ilog("Preparing ${n} workers to store index holding `${d}' items.", ("d", indexDescription)("n", workerCount));

        bfs::path outputPath = prepare_index_storage_path(indexDescription);
        bfs::create_directories(outputPath);

        for (size_t i = 0; i < workerCount; ++i)
        {
          std::string fileName = std::to_string(left) + '_' + std::to_string(right) + ".sst";
          bfs::path actualOutputPath(outputPath);
          actualOutputPath /= fileName;

          _builtWorkers.emplace_back(std::make_unique<dumping_worker>(actualOutputPath, *this, left, right, _is_error));

          retVal.emplace_back(_builtWorkers.back().get());

          left = right + 1;
          if (i == workerCount - 2) /// Last iteration shall cover items up to container end.
            right = lastId + 1;
          else
            right += ITEMS_PER_WORKER;
        }

        return retVal;
      }

      void index_dump_writer::start(const workers &workers)
      {
        FC_ASSERT(_builtWorkers.size() == workers.size());

        const size_t num_threads = _allow_concurrency ? std::min(workers.size(), static_cast<size_t>(16)) : 1;

        if (num_threads > 1)
        {
          boost::asio::io_service ioService;
          boost::thread_group threadpool;
          std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(ioService);

          for (unsigned int i = 0; i < num_threads; ++i)
            threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

          for (size_t i = 0; i < _builtWorkers.size(); ++i)
          {
            dumping_worker *w = _builtWorkers[i].get();
            FC_ASSERT(w == workers[i]);

            ioService.post(boost::bind(&dumping_worker::perform_dump, w));
          }

          ilog("Waiting for dumping-workers jobs completion");

          /// Run the horses...
          work.reset();

          threadpool.join_all();
        }
        else
        {
          for (size_t i = 0; i < _builtWorkers.size(); ++i)
          {
            dumping_worker *w = _builtWorkers[i].get();
            FC_ASSERT(w == workers[i]);

            w->perform_dump();
          }
        }
      }

      void index_dump_writer::store_index_manifest(index_manifest_info *manifest) const
      {
        if (process_index(_indexDescription) == false)
          return;

        FC_ASSERT(_processingSuccess);

        manifest->name = _indexDescription;
        manifest->dumpedItems = _index.size();
        manifest->firstId = _firstId;
        manifest->lastId = _lastId;
        manifest->indexNextId = _nextId;

        size_t totalWrittenEntries = 0;

        for (size_t i = 0; i < _builtWorkers.size(); ++i)
        {
          const dumping_worker *w = _builtWorkers[i].get();
          w->store_index_manifest(manifest);
          size_t writtenEntries = 0;
          w->is_write_finished(&writtenEntries);

          totalWrittenEntries += writtenEntries;
        }

        FC_ASSERT(_index.size() == totalWrittenEntries, "Mismatch between written entries: ${e} and size ${s} of index: `${i}",
                  ("e", totalWrittenEntries)("s", _index.size())("i", _indexDescription));

        ilog("Saved manifest for index: '${d}' containing ${s} items and ${n} saved as next_id", ("d", _indexDescription)("s", manifest->dumpedItems)("n", manifest->indexNextId));
      }

      void index_dump_writer::safe_spawn_snapshot_dump(const chainbase::abstract_index *idx)
      {
        try
        {
          set_processing_success(false);
          idx->dump_snapshot(*this);
          set_processing_success(true);
        }
        catch (boost::interprocess::bad_alloc &ex)
        {
          wlog("Problem with a snapshot allocation. A value of `shared-file-size` option has to be greater or equals to a size of snapshot data...");
          wlog("${details}", ("details", ex.what()));

          wlog("During a snapshot writing an error is detected. Writing another indexes will be stopped as soon as possible");
          _is_error.store(true);

          _exception = std::current_exception();
          /*
            https://www.boost.org/doc/libs/1_74_0/doc/html/thread/thread_management.html

            A running thread can be interrupted by invoking the interrupt() member function of the corresponding boost::thread object.
            When the interrupted thread next executes one of the specified interruption points
            (or if it is currently blocked whilst executing one) with interruption enabled,
            then a boost::thread_interrupted exception will be thrown in the interrupted thread.
            Unless this exception is caught inside the interrupted thread's thread-main function,
            the stack unwinding process (as with any other exception) causes the destructors with automatic storage duration to be executed.
            Unlike other exceptions, when boost::thread_interrupted is propagated out of thread-main function, this does not cause the call to std::terminate
          */
          throw boost::thread_interrupted();
        }
        catch (fc::exception &e)
        {
          wlog("Problem with a snapshot writing.");
          wlog("${e}", (e));

          wlog("During a snapshot writing an error is detected. Writing another indexes will be stopped as soon as possible");
          _is_error.store(true);

          _exception = std::current_exception();
          throw boost::thread_interrupted();
        }
        catch (...)
        {
          wlog("During a snapshot writing an error is detected. Writing another indexes will be stopped as soon as possible");
          _is_error.store(true);

          _exception = std::current_exception();
          throw boost::thread_interrupted();
        }
      }

      void loading_worker::load_converted_data(worker_common_base::serialized_object_cache *cache)
      {
        FC_ASSERT(_entryIt);

        const size_t maxSize = get_serialized_object_cache_max_size();
        cache->reserve(maxSize);
        for (size_t n = 0; _entryIt->Valid() && n < maxSize; _entryIt->Next(), ++n)
        {

          if (BOOST_UNLIKELY(_is_error.load()))
          {
            FC_ASSERT(false, "During a snapshot loading an error is detected in ${s}. Loading is stopped.", ("s", _manifestInfo.name));
          }

          auto key = _entryIt->key();
          auto value = _entryIt->value();

          const size_t *keyId = reinterpret_cast<const size_t *>(key.data());
          FC_ASSERT(sizeof(size_t) == key.size());

          cache->emplace_back(*keyId, std::vector<char>());
          cache->back().second.insert(cache->back().second.end(), value.data(), value.data() + value.size());
        }

        if (cache->empty())
        {
          ilog("No objects loaded from storage for index: `${i}'", ("i", _manifestInfo.name));
        }
        else
        {
          size_t b = cache->front().first;
          size_t e = cache->back().first;

          ilog("Loaded objects from range <${b}, ${e}> for index: `${i}'", ("b", b)("e", e)("i", _manifestInfo.name));
        }
      }

      void loading_worker::perform_load()
      {
        if (_manifestInfo.dumpedItems == 0)
        {
          ilog("Snapshot data contains empty-set stored for index: `${i}.", ("i", _manifestInfo.name));
          return;
        }

        for (const auto &fileInfo : _manifestInfo.storage_files)
        {
          _reader = std::make_unique<::rocksdb::SstFileReader>(_controller.get_storage_config());

          bfs::path sstFilePath(_inputPath);
          sstFilePath /= fileInfo.relative_path;

          auto status = _reader->Open(sstFilePath.string());
          if (status.ok())
          {
            ilog("Successfully opened index SST file at path: `${p}'", ("p", sstFilePath.string()));
          }
          else
          {
            elog("Cannot open snapshot index SST file at path: `${p}'. Error details: `${e}'.", ("p", sstFilePath.string())("e", status.ToString()));
            throw std::exception();
          }

          ::rocksdb::ReadOptions rOptions;
          _entryIt.reset(_reader->NewIterator(rOptions));
          _entryIt->SeekToFirst();

          auto converter = _controller.get_converter();
          converter(this);

          _entryIt.reset();
          _reader.reset();

          ilog("Finished processing of SST file at path: `${p}'", ("p", sstFilePath.string()));
        }
      }

      chainbase::snapshot_reader::workers
      index_dump_reader::prepare(const std::string &indexDescription, snapshot_converter_t converter, size_t *snapshot_index_next_id, size_t *snapshot_dumped_items)
      {
        _converter = converter;
        _indexDescription = indexDescription;

        index_manifest_info key;
        key.name = indexDescription;
        auto snapshotIt = _snapshotManifest.find(key);

        if (snapshotIt == _snapshotManifest.end())
        {
          elog("chainbase index `${i}' has no data saved in the snapshot. chainbase index cleared, but no data loaded.", ("i", indexDescription));
          return workers();
        }

        const index_manifest_info &manifestInfo = *snapshotIt;

        *snapshot_index_next_id = manifestInfo.indexNextId;
        *snapshot_dumped_items = manifestInfo.dumpedItems;

        _builtWorkers.emplace_back(std::make_unique<loading_worker>(manifestInfo, _rootPath, *this, _is_error));

        workers retVal;
        retVal.emplace_back(_builtWorkers.front().get());

        return retVal;
      }

      void index_dump_reader::start(const workers &workers)
      {
        FC_ASSERT(_builtWorkers.size() == workers.size());

        for (size_t i = 0; i < _builtWorkers.size(); ++i)
        {
          loading_worker *w = _builtWorkers[i].get();
          FC_ASSERT(w == workers[i]);
          currentWorker = w;
          w->perform_load();
        }
      }

      size_t index_dump_reader::getCurrentlyProcessedId() const
      {
        return currentWorker->getProcessedId();
      }

      void index_dump_reader::safe_spawn_snapshot_load(chainbase::abstract_index *idx)
      {
        try
        {
          set_processing_success(false);
          idx->load_snapshot(*this);
          set_processing_success(true);
        }
        catch (boost::interprocess::bad_alloc &ex)
        {
          wlog("Problem with a snapshot allocation. A value of `shared-file-size` option has to be greater or equals to a size of snapshot data...");
          wlog("${details}", ("details", ex.what()));
          wlog("index description: ${idx_desc} id: ${id}", ("idx_desc", getIndexDescription())("id", getCurrentlyProcessedId()));

          wlog("During a snapshot loading an error is detected. Loading another indexes will be stopped as soon as possible");
          _is_error.store(true);

          _exception = std::current_exception();
          /*
            https://www.boost.org/doc/libs/1_74_0/doc/html/thread/thread_management.html

            A running thread can be interrupted by invoking the interrupt() member function of the corresponding boost::thread object.
            When the interrupted thread next executes one of the specified interruption points
            (or if it is currently blocked whilst executing one) with interruption enabled,
            then a boost::thread_interrupted exception will be thrown in the interrupted thread.
            Unless this exception is caught inside the interrupted thread's thread-main function,
            the stack unwinding process (as with any other exception) causes the destructors with automatic storage duration to be executed.
            Unlike other exceptions, when boost::thread_interrupted is propagated out of thread-main function, this does not cause the call to std::terminate
          */
          throw boost::thread_interrupted();
        }
        catch (fc::exception &e)
        {
          wlog("Problem with a snapshot loading.");
          wlog("${e}", (e));
          wlog("index description: ${idx_desc} id: ${id}", ("idx_desc", getIndexDescription())("id", getCurrentlyProcessedId()));

          wlog("During a snapshot loading an error is detected. Loading another indexes will be stopped as soon as possible");
          _is_error.store(true);

          _exception = std::current_exception();
          throw boost::thread_interrupted();
        }
        catch (...)
        {
          wlog("index description: ${idx_desc} id: ${id}", ("idx_desc", getIndexDescription())("id", getCurrentlyProcessedId()));

          wlog("During a snapshot loading an error is detected. Loading another indexes will be stopped as soon as possible");
          _is_error.store(true);

          _exception = std::current_exception();
          throw boost::thread_interrupted();
        }
      }

    }
  }
} // hive::plugins::state_snapshot