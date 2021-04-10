#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <boost/mpl/vector.hpp>
#include <type_traits>
#include <typeinfo>

namespace hive { namespace chain {

using boost::multi_index::multi_index_container;
using boost::multi_index::indexed_by;
using boost::multi_index::ordered_unique;
using boost::multi_index::tag;
using boost::multi_index::member;
using boost::multi_index::composite_key;
using boost::multi_index::composite_key_compare;
using boost::multi_index::const_mem_fun;

template< class Iterator >
inline boost::reverse_iterator< Iterator > make_reverse_iterator( Iterator iterator )
{
  return boost::reverse_iterator< Iterator >( iterator );
}

} } // hive::chain
