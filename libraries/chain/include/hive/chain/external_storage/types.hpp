#pragma once

#include <hive/protocol/types.hpp>

#include <rocksdb/slice.h>
#include <rocksdb/options.h>
#include <rocksdb/db.h>

#include <boost/core/demangle.hpp>

#include <hive/chain/external_storage/utilities.hpp>

namespace hive { namespace chain {

using hive::protocol::account_name_type;
using hive::protocol::transaction_id_type;

using ::rocksdb::Slice;
using ::rocksdb::Comparator;

#define STORE_MAJOR_VERSION          1
#define STORE_MINOR_VERSION          1

#define OPEN_FILE_LIMIT 750

#define checkStatus(s) FC_ASSERT((s).ok(), "Data access failed: ${m}", ("m", (s).ToString()))

/** Because localtion_id_pair stores block_number paired with operation_id_vop_pair, which stores operation id on 63 bits,
  *  max allowed operation-id is max_int64 (instead of max_uint64).
  */
#define MAX_OPERATION_ID             std::numeric_limits<int64_t>::max()

enum ColumnTypes
{
  COMMENT = 0,
  ACCOUNT_METADATA,
  ACCOUNT_AUTHORITY,
  ACCOUNT,
  ACCOUNT_BY_ID,
  ACCOUNT_BY_NEXT_VESTING_WITHDRAWAL
};

enum Columns
{
  CURRENT_LIB = 1,
  LAST_REINDEX_POINT = CURRENT_LIB,
  OPERATION_BY_ID,
  OPERATION_BY_BLOCK,
  AH_INFO_BY_NAME,
  AH_OPERATION_BY_ID,
  BY_TRANSACTION_ID
};

/** Helper class to simplify construction of Slice objects holding primitive type values.
  *
  */
template <typename T>
class PrimitiveTypeSlice final : public Slice
{
  public:
    explicit PrimitiveTypeSlice(T value) : _value(value)
    {
      data_ = reinterpret_cast<const char*>(&_value);
      size_ = sizeof(T);
    }

    static const T& unpackSlice(const Slice& s)
    {
      assert(sizeof(T) == s.size());
      return *reinterpret_cast<const T*>(s.data());
    }

    static const T& unpackSlice(const std::string& s)
    {
      assert(sizeof(T) == s.size());
      return *reinterpret_cast<const T*>(s.data());
    }

  private:
    T _value;
};

class TransactionIdSlice : public Slice
{
  public:
    explicit TransactionIdSlice(const transaction_id_type& trxId) : _trxId(&trxId)
    {
      data_ = _trxId->data();
      size_ = _trxId->data_size();
    }

    static void unpackSlice(const Slice& s, transaction_id_type* storage)
    {
      assert(storage != nullptr);
      assert(storage->data_size() == s.size());
      memcpy(storage->data(), s.data(), s.size());
    }

  private:
    const transaction_id_type* _trxId;
};

typedef PrimitiveTypeSlice< int64_t > id_slice_t;
typedef PrimitiveTypeSlice< uint32_t > lib_id_slice_t;
typedef PrimitiveTypeSlice< uint32_t > lib_slice_t;

#define LIB_ID lib_id_slice_t( 0 )
#define REINDEX_POINT_ID lib_id_slice_t( 1 )

/** Helper base class to cover all common functionality across defined comparators.
  *
  */
class AComparator : public Comparator
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

class HashComparator final : public AComparator
{
  public:
    virtual int Compare(const Slice& a, const Slice& b) const override
    {
      /// Nothing more to do. Just compare buffers holding 20Bytes hash
      return a.compare(b);
    }

    virtual bool Equal(const Slice& a, const Slice& b) const override
    {
      return a == b;
    }
};

using TransactionIdComparator = HashComparator;

struct operation_id_vop_pair
{
  operation_id_vop_pair(uint64_t id = 0, bool is_virtual_op = false) : _op_id(id), _is_virtual_op(is_virtual_op)
  {
    FC_ASSERT(id < static_cast<uint64_t>(MAX_OPERATION_ID));
  }

  bool operator < (const operation_id_vop_pair& rhs) const
  {
    return _op_id < rhs._op_id; /// Intentionally skipped _is_virtual_op field, which holds only information about pointed operation
  }

  bool operator > (const operation_id_vop_pair& rhs) const
  {
    return _op_id > rhs._op_id; /// Intentionally skipped _is_virtual_op field, which holds only information about pointed operation
  }

  bool operator == (const operation_id_vop_pair& rhs) const
  {
    return _op_id == rhs._op_id; /// Intentionally skipped _is_virtual_op field, which holds only information about pointed operation
  }

  uint64_t get_id() const { return _op_id; }
  bool is_virtual() const { return _is_virtual_op; }

private:
  uint64_t _op_id : 63;
  uint64_t _is_virtual_op : 1;
};


/** Location index is nonunique. Since RocksDB supports only unique indexes, it must be extended
  *  by some unique part (ie ID).
  *
  */
typedef std::pair< uint32_t, operation_id_vop_pair > block_op_id_pair;

typedef PrimitiveTypeComparatorImpl<uint32_t> by_id_ComparatorImpl;
typedef PrimitiveTypeComparatorImpl< block_op_id_pair > op_by_block_num_ComparatorImpl;
typedef PrimitiveTypeComparatorImpl<account_name_type::Storage> by_account_name_ComparatorImpl;

/// Compares account_history_info::id and rocksdb_operation_object::id pair
typedef std::pair< int64_t, uint32_t > ah_op_id_pair;
typedef PrimitiveTypeComparatorImpl< ah_op_id_pair > ah_op_by_id_ComparatorImpl;

/// Pairs account_name storage type and the ID to make possible nonunique index definition over names.
typedef std::pair<account_name_type::Storage, size_t> account_name_storage_id_pair;

typedef PrimitiveTypeSlice< block_op_id_pair > op_by_block_num_slice_t;
typedef PrimitiveTypeSlice< uint32_t > by_block_slice_t;
typedef PrimitiveTypeSlice< account_name_type::Storage > info_by_name_slice_t;
typedef PrimitiveTypeSlice< ah_op_id_pair > ah_op_by_id_slice_t;

typedef std::pair<uint32_t, uint32_t> block_no_tx_in_block_pair;
typedef PrimitiveTypeSlice<block_no_tx_in_block_pair> block_no_tx_in_block_slice_t;

const Comparator* by_Hash_Comparator();
const Comparator* by_id_Comparator();
const Comparator* op_by_block_num_Comparator();
const Comparator* by_account_name_Comparator();
const Comparator* ah_op_by_id_Comparator();
const Comparator* by_txId_Comparator();

/** Represents an AH entry in mapped to account name.
  *  Holds additional informations, which are needed to simplify pruning process.
  *  All operations specific to given account, are next mapped to ID of given object.
  */
class account_history_info
{
public:
  int64_t        id = 0;
  uint32_t       oldestEntryId = 0;
  uint32_t       newestEntryId = 0;
  /// Timestamp of oldest operation, just to quickly decide if start detail prune checking at all.
  time_point_sec oldestEntryTimestamp;

  uint32_t getAssociatedOpCount() const
  {
    return newestEntryId - oldestEntryId + 1;
  }
};

using ::rocksdb::DB;
using ::rocksdb::ColumnFamilyHandle;
using ::rocksdb::WriteBatch;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ReadOptions;

class CachableWriteBatch : public WriteBatch
{
public:
  CachableWriteBatch(const std::unique_ptr<DB>& storage, const std::vector<ColumnFamilyHandle*>& columnHandles)
    : _storage(storage), _columnHandles(columnHandles) {}

  bool getAHInfo(const account_name_type& name, account_history_info* ahInfo) const;
  void putAHInfo(const account_name_type& name, const account_history_info& ahInfo);

  void Clear();

private:
  const std::unique_ptr<DB>&                        _storage;
  const std::vector<ColumnFamilyHandle*>&           _columnHandles;
  std::map<account_name_type, account_history_info> _ahInfoCache;
};

} } // hive::chain


FC_REFLECT( hive::chain::account_history_info,
  (id)(oldestEntryId)(newestEntryId)(oldestEntryTimestamp) )
