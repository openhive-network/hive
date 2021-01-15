// #include "stdafx.h"

#include <boost/mpl/for_each.hpp>
#include <boost/mpl/inserter.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/copy_if.hpp>
#include <boost/mpl/not.hpp>
#include <boost/mpl/transform_view.hpp>

#include <boost/core/demangle.hpp>
#include <boost/type.hpp>
#include <boost/tuple/tuple.hpp>

#include <hive/protocol/operations.hpp>

namespace type_extractor
{

	namespace mpl = boost::mpl;

	enum
	{
		SPLIT_LIMIT = 20
	};

	template <bool limit_exceeded, typename... Args>
	struct splitter_impl
	{
	};

	template <typename... Args>
	struct splitter_impl<false, Args...>
	{
		typedef typename mpl::list<Args...>::type type_container;
		typedef type_container continuous_view;
	};

	template <typename T1, typename T2, typename T3, typename T4, typename T5,
						typename T6, typename T7, typename T8, typename T9, typename T10,
						typename T11, typename T12, typename T13, typename T14, typename T15,
						typename T16, typename T17, typename T18, typename T19, typename T20,
						typename... Args>
	struct splitter_impl<true, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, Args...>
	{
		static constexpr bool limit_exceeded = sizeof...(Args) > SPLIT_LIMIT;

		typedef splitter_impl<limit_exceeded, Args...> next_chunk_producer;
		typedef typename mpl::list<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20>::type type_container;
		typedef typename mpl::joint_view<type_container, typename next_chunk_producer::continuous_view>::type continuous_view;
	};

	template <typename... Args>
	struct splitter
	{
		static constexpr bool limit_exceeded = sizeof...(Args) > SPLIT_LIMIT;
		typedef typename splitter_impl<limit_exceeded, Args...>::continuous_view continuous_view;
	};

	template <class... L>
	struct ul_join_impl;

	template <class... L>
	using ul_join = typename ul_join_impl<L...>::type;

	template <class... T>
	struct unlimited_list
	{
		typedef unlimited_list<T...> type;
	};

	template <>
	struct ul_join_impl<>
	{
		using type = unlimited_list<>;
	};

	template <class... L, class... T>
	struct ul_join_impl<unlimited_list<L...>, T...>
	{
		typedef unlimited_list<L..., T...> type;
	};

	template <class L>
	struct sv_subtype_producer;

	template <typename... T>
	struct sv_subtype_producer<unlimited_list<T...>>
	{
		typedef fc::static_variant<T...> type;
	};

	template <class T, typename = typename T::type_infos>
	class sv_processor;

	/** Each type used in source `static_variant` shall be transformed into tuple holding this processed type
    and `static_variant` instantiation without given type.
    For example:
    fc::static_variant<A, B, C>
    will result in transformation:

    (A, fc::static_variant<B, C>)
    (B, fc::static_variant<A, C>)
    (C, fc::static_variant<A, B>)
*/
	template <class T, typename... Args>
	class sv_processor<T, ::fc::impl::type_info<Args...>>
	{
	public:
		typedef typename splitter<Args...>::continuous_view source_type_list;

		struct type_appender
		{
			template <class L, typename Y>
			struct apply
			{
				typedef typename ul_join<L, Y>::type type;
			};
		};

		struct type_transformer
		{
			template <typename ProcessedType>
			struct apply
			{
				typedef typename mpl::copy_if<source_type_list, typename mpl::not_<boost::is_same<ProcessedType, mpl::_>>,
																			typename mpl::inserter<unlimited_list<>, type_appender>>::type subtype_list;
				typedef typename sv_subtype_producer<subtype_list>::type sv_subtype;
				typedef typename boost::tuple<ProcessedType, sv_subtype> type;
			};
		};

		typedef typename mpl::transform_view<source_type_list, type_transformer>::type transformed_type_list;
	};

} // namespace type_extractor