#pragma once

#include <hive/protocol/fixed_string.hpp>
#include <chainbase/util/object_id.hpp>
#include <chainbase/util/object_id_serialization.hpp>

#include <boost/core/demangle.hpp>

#include <fc/io/datastream.hpp>
#include <fc/io/raw_fwd.hpp>

#include <functional>
#include <string>
#include <vector>

namespace chainbase
{

namespace serialization
{

template< typename T, typename B >
inline void pack_to_buffer(B& raw, const T& v)
  {
  auto size = fc::raw::pack_size(v);
  raw.resize(size);
  fc::datastream< char* > ds(raw.data(), size);
  fc::raw::pack(ds, v);
  }

template< typename T, typename B >
inline void unpack_from_buffer(T& v, const B& raw)
  {
  fc::datastream<const char*> ds(raw.data(), raw.size());
  fc::raw::unpack(ds, v);
  }

} /// namespace serialization


class snapshot_base_serializer
  {
  public:
    class worker_data
      {
      protected:
        worker_data() = default;
        ~worker_data() = default;
      };

    class worker_common_base
      {
      public:
        typedef std::pair<size_t, std::vector<char>> id_serialized_object;
        typedef std::vector< id_serialized_object> serialized_object_cache;

        size_t get_serialized_object_cache_max_size() const;

        std::pair<size_t, size_t> get_processing_range() const
          {
          return std::make_pair(_startId, _endId);
          }

        void associate_data(worker_data& data)
          {
          _worker_data = &data;
          }

        worker_data& get_associated_data() const
          {
          return *_worker_data;
          }

      protected:
        worker_common_base(size_t startId, size_t endId) :
          _worker_data(nullptr), _startId(startId), _endId(endId) {}

      protected:
        worker_data* _worker_data;
        size_t _startId;
        size_t _endId;
      };

  protected:
    snapshot_base_serializer() = default;
    ~snapshot_base_serializer() = default;
  };

class snapshot_writer : public snapshot_base_serializer
{
public:
  class worker : public worker_common_base
  {
  public:
    virtual void flush_converted_data(const worker_common_base::serialized_object_cache& cache) = 0;
    virtual std::string prettifyObject(const fc::variant& object, const std::vector<char>& buffer) const = 0;

  protected:
    worker(chainbase::snapshot_writer& controller, size_t startId, size_t endId) :
      worker_common_base(startId, endId),
      _controller(controller) {}
    virtual ~worker() {}

  private:
    chainbase::snapshot_writer& _controller;
  };

  using snapshot_converter_t = std::function <void(worker*)>;

  typedef std::vector<worker*> workers;

  virtual workers prepare(const std::string& indexDescription, size_t firstId, size_t lastId, size_t indexSize, size_t indexNextId, snapshot_converter_t converter) = 0;
  virtual void start(const workers& workers) = 0;

protected:
  virtual ~snapshot_writer() {}
};

class snapshot_reader : public snapshot_base_serializer
  {
  public:
    class worker : public worker_common_base
      {
      public:
        /** Allows to load data from snapshot into the cache. 
        */
        virtual void load_converted_data(worker_common_base::serialized_object_cache* cache) = 0;
        virtual std::string prettifyObject(const fc::variant& object, const std::vector<char>& buffer) const = 0;

        void update_processed_id(size_t id)
          {
          _processedId = id;
          }

        size_t getProcessedId() const
          {
          return _processedId;
          }

      protected:
        worker(chainbase::snapshot_reader& controller, size_t startId, size_t endId) :
          worker_common_base(startId, endId),
          _controller(controller),
          _processedId(0) {}
        virtual ~worker() {}

      private:
        chainbase::snapshot_reader& _controller;
        size_t _processedId;
      };

    using snapshot_converter_t = std::function <void(worker*)>;

    typedef std::vector<worker*> workers;

    virtual workers prepare(const std::string& indexDescription, snapshot_converter_t converter, size_t* snapshot_index_next_id) = 0;
    virtual void start(const workers& workers) = 0;

  protected:
    virtual ~snapshot_reader() {}
  };

class generic_index_serialize_base
  {
  protected:
    template <class MultiIndexType>
    std::string get_index_name() const
      {
      std::string indexName = boost::core::demangle(typeid(typename MultiIndexType::value_type).name());

      auto pos = indexName.rfind(':');
      if(pos != std::string::npos)
        indexName = indexName.substr(pos + 1);

      return indexName;
      }
  };

/** interface should be implemented by each item which should be part of snapshot.
*/
template <class GenericIndexType>
class generic_index_snapshot_dumper final : public generic_index_serialize_base
{
public:
  using id_type = typename GenericIndexType::id_type;

  generic_index_snapshot_dumper(const GenericIndexType& index, snapshot_writer& writer) :
    _index(index),
    _writer(writer) {}

  void dump(id_type index_next_id) const
    {
    dump_index(index_next_id, _index.indices());
    }

private:
  template <class MultiIndexType>
  class dumper_data final : public snapshot_writer::worker_data
  {
  public:
    dumper_data(const MultiIndexType& multiIndex, const snapshot_writer::worker* worker, const std::string& indexDescription) :
      _data_source(multiIndex),
      _indexDescription(indexDescription)
    {
      auto iterationRange = worker->get_processing_range();
      _startId = iterationRange.first;
      _endId = iterationRange.second;

      const auto& byIdIdx = multiIndex.template get<by_id>();

      typedef typename MultiIndexType::value_type::id_type id_type;

      _start = byIdIdx.lower_bound(id_type(iterationRange.first));
      _end   = byIdIdx.upper_bound(id_type(iterationRange.second));

      if(_start != _end && _start != byIdIdx.end())
      {
        const auto& object = *_start;
        size_t id = object.get_id();
        if(id > iterationRange.second)
          _start = _end;
      }
    }

    virtual ~dumper_data() = default;

    void doConversion(snapshot_writer::worker* worker) const
      {
      if(_start == _end)
        {
        ilog("No items present for range <${b}, ${e}> for index `${i}", ("b", _startId)("e", _endId)("i", _indexDescription));
        return;
        }

      const auto& byIdIdx = _data_source.template get<by_id>();

      size_t firstObjectId = _start->get_id();
      if(_end != byIdIdx.end())
        {
        size_t endObjectId = _end->get_id();
        ilog("Writing items <${f}, ${l}) found for range <${b}, ${e}) from index ${s}", ("b", _startId)
          ("e", _endId)("s", _indexDescription)("f", firstObjectId)("l", endObjectId));
        }
      else
        {
        ilog("Writing items <${f}, container-end) found for range <${b}, ${e}) from index ${s}", ("b", _startId)
          ("e", _endId)("s", _indexDescription)("f", firstObjectId));
        }

      const uint32_t max_cache_size = worker->get_serialized_object_cache_max_size();

      snapshot_writer::worker::serialized_object_cache serializedCache;
      serializedCache.reserve(max_cache_size);

      for(auto indexIt = _start; indexIt != _end; ++indexIt)
      {
        const auto& object = *indexIt;

        size_t id = object.get_id();

        FC_ASSERT(id >= _startId && id <= _endId, "Processing object-id: ${i} from different id-range: <${l},${r})",
          ("i", id)("l", _startId)("r", _endId));

        serializedCache.emplace_back(id, std::vector<char>());
        serialization::pack_to_buffer(serializedCache.back().second, object);

        //std::string dump = worker->prettifyObject(fc::variant(object), serializedCache.back().second);
        //if(dump.empty() == false)
        //  ilog("Dumping object: id: ${id}, `${o}'", ("id", id)("o", dump));

        if(serializedCache.size() >= max_cache_size)
        {
          worker->flush_converted_data(serializedCache);
          serializedCache.clear();
        }
      }

      if(serializedCache.empty() == false)
        worker->flush_converted_data(serializedCache);

      ilog("Finished dumping items <${b}, ${e}> from ${s}", ("b", _startId)("e", _endId)("s", _indexDescription));
    }

  private:

    typedef typename MultiIndexType::iterator iterator;
    const MultiIndexType& _data_source;
    iterator _start;
    iterator _end;
    size_t _startId;
    size_t _endId;

    std::string _indexDescription;
  };

  template <class MultiIndexType>
  void dump_index(id_type index_next_id, const MultiIndexType& index) const
  {
    typedef dumper_data< MultiIndexType> dumper_t;

    std::string indexName = this->template get_index_name<MultiIndexType>();

    auto converter = [](snapshot_writer::worker* w) -> void
      {
      snapshot_writer::worker_data& associatedData = w->get_associated_data();
      dumper_t* actualData = static_cast<dumper_t*>(&associatedData);
      actualData->doConversion(w);
      };

    size_t firstId = 0;
    size_t lastId = 0;
    const auto& byIdIdx = index.template get<by_id>();

    if(byIdIdx.empty() == false)
      {
      firstId = byIdIdx.begin()->get_id();
      lastId = byIdIdx.rbegin()->get_id();
      }

    auto workers = _writer.prepare(indexName, firstId, lastId, index.size(), index_next_id, converter);

    std::vector<std::unique_ptr<dumper_t>> workerData;

    for(auto* w : workers)
    {
      workerData.emplace_back(std::make_unique<dumper_t>(index, w, indexName));
      w->associate_data(*workerData.back());
    }

    _writer.start(workers);
  }

private:
  const GenericIndexType& _index;
  snapshot_writer& _writer;
};

template <class GenericIndexType>
class generic_index_snapshot_loader final : public generic_index_serialize_base
  {
  public:
    using id_type = typename GenericIndexType::id_type;

    generic_index_snapshot_loader(GenericIndexType& index, snapshot_reader& reader) :
      _index(index),
      _reader(reader) {}

    /// <summary>
    /// Allows to load index contents from snapshot. 
    /// Returns index next_id value.
    /// </summary>
    id_type load()
      {
      return load_index(_index.mutable_indices());
      }

  private:
    template <class MultiIndexType>
    class loader_data final : public snapshot_reader::worker_data
      {
      public:
        loader_data(GenericIndexType& genericIndex, MultiIndexType& multiIndex, const snapshot_reader::worker* worker, const std::string& indexDescription) :
          _generic_index(genericIndex),
          _data_source(multiIndex),
          _indexDescription(indexDescription)
          {
          }

        virtual ~loader_data() = default;

        void doConversion(snapshot_reader::worker* worker)
          {
          typedef typename MultiIndexType::value_type::id_type id_type;

          const uint32_t max_cache_size = worker->get_serialized_object_cache_max_size();
          snapshot_writer::worker::serialized_object_cache serializedCache;
          serializedCache.reserve(max_cache_size);

          while(1)
            {
            serializedCache.clear();
            worker->load_converted_data(&serializedCache);

            if(serializedCache.empty())
              break;

            size_t f = serializedCache.front().first;
            size_t l = serializedCache.back().first;

            ilog("Loading items <${b}, ${e}> from ${s}", ("b", f)("e", l)("s", _indexDescription));

            for(const auto& buffer : serializedCache)
              {
              /// Just to catch loading context on some caught exception.
              worker->update_processed_id(buffer.first);

              auto prettyDump = [&buffer, worker](fc::variant object) -> std::string
              {
                return worker->prettifyObject(object, buffer.second);
              };

              _generic_index.unpack_from_snapshot(id_type(buffer.first),
                [this, &buffer](typename MultiIndexType::value_type& object)
                {
                serialization::unpack_from_buffer(object, buffer.second);
                },
                std::move(prettyDump)
                );
              }

            ilog("Finished loading items <${b}, ${e}> from ${s}", ("b", f)("e", l)("s", _indexDescription));
            }
          }

      private:
        GenericIndexType& _generic_index;
        MultiIndexType& _data_source;
        std::string _indexDescription;
      };

    template <class MultiIndexType>
    id_type load_index(MultiIndexType& index)
      {
      typedef loader_data<MultiIndexType> loader_t;

      std::string indexName = this->template get_index_name<MultiIndexType>();

      auto converter = [](snapshot_reader::worker* w) -> void
        {
        snapshot_reader::worker_data& associatedData = w->get_associated_data();
        loader_t* actualData = static_cast<loader_t*>(&associatedData);
        actualData->doConversion(w);
        };

      size_t index_next_id = 0;

      auto workers = _reader.prepare(indexName, converter, &index_next_id);

      std::vector<std::unique_ptr<loader_t>> workerData;

      for(auto* w : workers)
        {
        workerData.emplace_back(std::make_unique<loader_t>(_index, index, w, indexName));
        w->associate_data(*workerData.back());
        }

      _reader.start(workers);

      return id_type(index_next_id);
      }

  private:
    GenericIndexType& _index;
    snapshot_reader&  _reader;
  };

} /// namespace chainbase

