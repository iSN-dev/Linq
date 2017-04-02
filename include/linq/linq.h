#ifndef LINQ_H_INCLUDED
#define LINQ_H_INCLUDED

#include <memory>
#include <type_traits>
#include <functional>
#include <utility>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

namespace linq
{
	/* utils */
	enum class iterator_type
	{
		basic,
		reach,
		filter,
		load,
		full
	};

	template<typename Key, typename Value, bool is_basic_type>
	struct map_type
	{
		typedef std::unordered_map<Key, Value> type;
	};
	template<typename Key, typename Value>
	struct map_type<Key, Value, false>
	{
		typedef std::map<Key, Value> type;
	};

	template<typename In, typename KeyLoader = void, typename... Funcs>
	struct group_by
	{
		using Out = decltype(std::declval<KeyLoader>()(std::declval<In>()));
		typedef typename map_type<Out, typename group_by<In, Funcs...>::type, std::is_fundamental<Out>::value>::type type;

		inline static void emplace(type &handle, In const &val, KeyLoader const &func, Funcs const &...funcs) noexcept
		{
			group_by<In, Funcs...>::emplace(handle[func(val)], val, funcs...);
		}
	};
	template<typename In, typename KeyLoader>
	struct group_by<In, KeyLoader>
	{
		using Out = decltype(std::declval<KeyLoader>()(std::declval<In>()));
		typedef typename map_type<Out, typename group_by<In>::type, std::is_fundamental<Out>::value>::type type;

		inline static void emplace(type &handle, In const &val, KeyLoader const &func) noexcept
		{
			group_by<In>::emplace(handle[func(val)], val);
		}
	};
	template<typename In>
	struct group_by<In>
	{
		typedef std::vector<typename std::remove_reference<In>::type> type;

		inline static void emplace(type &vec, In const &val) noexcept
		{
			vec.push_back(val);
		}
	};

	enum class order_type
	{
		asc,
		desc
	};
	template<typename Key, order_type OrderType = order_type::asc>
	struct order_by_modifier
	{
		typedef Key key_type;
		static constexpr order_type ob_type = OrderType;

		Key const &key_;
		order_by_modifier(Key const &key) : key_(key) {}
	};

	template <typename Key>
	auto const asc(Key const &key) noexcept { return std::move(order_by_modifier<Key, order_type::asc>(key)); }
	template <typename Key>
	auto const desc(Key const &key) noexcept { return std::move(order_by_modifier<Key, order_type::desc>(key)); }

	template<typename In, typename Base>
	struct order_by_modifier_end : public Base
	{
		static constexpr order_type type = Base::ob_type;

		order_by_modifier_end() = delete;
		~order_by_modifier_end() = default;
		order_by_modifier_end(order_by_modifier_end const &) = default;
		order_by_modifier_end(Base const &key) : Base(key.key_) {}
		order_by_modifier_end(typename Base::key_type const &key) : Base(key) {}

		auto operator()(In val) const noexcept { return /*std::forward<In &&>(*/this->key_(val)/*)*/; }

	};


	template<typename In, order_type OrderType>
	struct order_by_less
	{
		inline bool operator()(const In a, const In b) const noexcept
		{ 
			return a < b;
		}
	};
	template<typename In>
	struct order_by_less<In, order_type::desc>
	{
		inline bool operator()(const In a, const In b) const  noexcept
		{
			return a > b;
		}
	};
	template<typename In, typename Key1, typename Key2, typename... Keys>
	inline bool order_by_next(In &a, In &b, Key1 const &key1, Key2 const &key2, Keys const &...keys) noexcept
	{
		return (key1(a) == key1(b) && order_by_less<decltype(key2(a)), Key2::type>()(key2(a), key2(b))) || order_by_next(a, b, key2, keys...);
	}
	template<typename In, typename Key1, typename Key2>
	inline bool order_by_next(In &a, In &b, Key1 const &key1, Key2 const &key2) noexcept
	{
		return key1(a) == key1(b) && order_by_less<decltype(key2(a)), Key2::type>()(key2(a), key2(b));
	}
	template<typename In, typename Key, typename... Keys>
	inline bool order_by(In &a, In &b, Key const &key, Keys const &...keys) noexcept
	{
		return order_by_less<decltype(key(a)), Key::type>()(key(a), key(b)) || order_by_next(a, b, key, keys...);
	}
	template<typename In, typename Key>
	inline bool order_by(In &a, In &b, Key const &key) noexcept
	{
		return order_by_less<decltype(key(a)), Key::type>()(key(a), key(b));
	}

	/*! utils */

	/* iterator */
	template<typename Proxy, typename Base, typename Out, iterator_type ItType>
	class iterator;

	template<typename Proxy, typename Base, typename Out>
	class iterator<Proxy, Base, Out, iterator_type::full> : public Base
	{
		Proxy const &proxy_;

	public:
		iterator() = delete;
		~iterator() = default;
		iterator(iterator const &) = default;
		iterator(Base const &IState, Proxy const &proxy)
			: Base(IState), proxy_(proxy)
		{}

		iterator const &operator++() noexcept
		{
			do
			{
				static_cast<Base &>(*this).operator++();
			} while (proxy_.validate(static_cast<Base const &>(*this)));
			return (*this);
		}

		inline Out operator*() const noexcept
		{
			return proxy_.load(static_cast<Base const &>(*this));
		}

		iterator const &operator=(iterator const &rhs)
		{
			static_cast<Base &>(*this) = static_cast<Base const &>(rhs);
			return (*this);
		}

		inline bool operator!=(iterator const &rhs) const noexcept {
			return static_cast<Base const &>(*this) != static_cast<Base const &>(rhs);
		}

	};

	template<typename Proxy, typename Base, typename Out>
	class iterator<Proxy, Base, Out, iterator_type::load> : public Base
	{
		Proxy const &proxy_;
	public:
		iterator() = delete;
		~iterator() = default;
		iterator(iterator const &) = default;
		iterator(Base const &IState, Proxy const &proxy)
			: Base(IState), proxy_(proxy)
		{}

		inline Out operator*() const noexcept { return proxy_.load(static_cast<Base const &>(*this)); }

		inline iterator const &operator++() noexcept {
			static_cast<Base &>(*this).operator++();
			return *this;
		}

		iterator const &operator=(iterator const &rhs)
		{
			static_cast<Base &>(*this) = static_cast<Base const &>(rhs);
			return (*this);
		}

		inline bool operator!=(iterator const &rhs) const noexcept {
			return static_cast<Base const &>(*this) != static_cast<Base const &>(rhs);
		}
	};

	template<typename Proxy, typename Base, typename Out>
	class iterator<Proxy, Base, Out, iterator_type::filter> : public Base
	{
		Proxy const &proxy_;
	public:
		iterator() = delete;
		~iterator() = default;
		iterator(iterator const &) = default;
		iterator(Base const &IState, Proxy const &proxy)
			: Base(IState), proxy_(proxy)
		{}

		inline Out operator*() const noexcept { return *static_cast<Base const &>(*this); }

		iterator const &operator++() noexcept {
			do
			{
				static_cast<Base &>(*this).operator++();
			} while (proxy_.validate(static_cast<Base const &>(*this)));
			return (*this);
		}

		iterator const &operator=(iterator const &rhs)
		{
			static_cast<Base &>(*this) = static_cast<Base const &>(rhs);
			return (*this);
		}

		inline bool operator!=(iterator const &rhs) const noexcept {
			return static_cast<Base const &>(*this) != static_cast<Base const &>(rhs);
		}
	};

	template<typename Proxy, typename Base, typename Out>
	class iterator<Proxy, Base, Out, iterator_type::reach> : public Base
	{
		Proxy const &proxy_;
	public:
		iterator() = delete;
		~iterator() = default;
		iterator(iterator const &) = default;
		iterator(Base const &IState, Proxy const &proxy)
			: Base(IState), proxy_(proxy)
		{}

		inline Out operator*() const noexcept { return *static_cast<Base const &>(*this); }

		iterator const &operator=(iterator const &rhs)
		{
			static_cast<Base &>(*this) = static_cast<Base const &>(rhs);
			return (*this);
		}

		inline bool operator!=(iterator const &rhs) const noexcept {
			return  static_cast<Base const &>(*this) != static_cast<Base const &>(rhs) && proxy_.reach(static_cast<Base const &>(*this));
		}

		iterator const &operator++() noexcept {
			proxy_.incr(static_cast<Base const &>(*this));
			static_cast<Base &>(*this).operator++();
			return (*this);
		}

	};

	template<typename Proxy, typename Base, typename Out>
	class iterator<Proxy, Base, Out, iterator_type::basic> : public Base
	{
	public:
		iterator() = delete;
		~iterator() = default;
		iterator(iterator const &) = default;
		iterator(Base const &IState, Proxy const &)
			: Base(IState)
		{}

		inline Out operator*() const noexcept { return *static_cast<Base const &>(*this); }

		inline iterator const &operator++() noexcept {
			static_cast<Base &>(*this).operator++();
			return *this;
		}

		iterator const &operator=(iterator const &rhs)
		{
			static_cast<Base &>(*this) = static_cast<Base const &>(rhs);
			return (*this);
		}

		inline bool operator!=(iterator const &rhs) const noexcept {
			return static_cast<Base const &>(*this) != static_cast<Base const &>(rhs);
		}
	};

	/*Linq statements*/
	template<typename Proxy, typename Base_It, typename Out, iterator_type ItType>
	class IState;
	//GroupBy
	template<typename Iterator, typename Proxy>
	class GroupBy : public IState
		<
		GroupBy<Iterator, Proxy>,
		Iterator,
		decltype(*std::declval<Iterator>()),
		iterator_type::basic
		>
	{

	public:
		using proxy_t = GroupBy<Iterator, Proxy>;
		using Out = decltype(*std::declval<Iterator>());
		using base_t = IState<proxy_t, Iterator, Out, iterator_type::basic>;

		using iterator_t = linq::iterator<proxy_t, Iterator, Out, iterator_type::basic>;

		typedef iterator_t iterator;
		typedef iterator_t const_iterator;

	private:
		Proxy proxy_;
	public:
		using last_map_t = decltype(*(proxy_->begin()));
		using last_key_t = decltype(std::declval<last_map_t>().first);

		GroupBy() = delete;
		GroupBy(GroupBy const &) = default;
		GroupBy(Iterator const &begin, Iterator const &end, Proxy proxy)
			: base_t(begin, end), proxy_(proxy)
		{}

		inline auto &operator[](last_key_t const &key) const
		{
			return (*proxy_)[key];
		}

	};
	//OrderBy
	template<typename Iterator, typename Proxy>
	class OrderBy : public IState
		<
		OrderBy<Iterator, Proxy>,
		Iterator,
		decltype(*std::declval<Iterator>()),
		iterator_type::basic
		>
	{

	public:
		using proxy_t = OrderBy<Iterator, Proxy>;
		using Out = decltype(*std::declval<Iterator>());
		using base_t = IState<proxy_t, Iterator, Out, iterator_type::basic>;

		using iterator_t = linq::iterator<proxy_t, Iterator, Out, iterator_type::basic>;

		typedef iterator_t iterator;
		typedef iterator_t const_iterator;

	private:
		Proxy proxy_;
	public:

		OrderBy() = delete;
		OrderBy(OrderBy const &) = default;
		OrderBy(Iterator const &begin, Iterator const &end, Proxy proxy)
			: base_t(begin, end), proxy_(proxy)
		{}

		inline auto asc() const noexcept { return *this; }
		inline auto desc() const noexcept { return OrderBy<decltype(proxy_->rbegin()), Proxy>(proxy_->rbegin(), proxy_->rend(), proxy_); }

		//inline auto &operator[](last_key_t const &key) const
		//{
		//	return (*proxy_)[key];
		//}

	};
	//SelectWhere
	template<typename Iterator, typename Filter, typename Loader>
	class SelectWhere
		: public IState
		<
		SelectWhere<Iterator, Filter, Loader>,
		Iterator,
		decltype(std::declval<Loader>()(std::declval<decltype(*std::declval<Iterator>())>())),
		iterator_type::full
		>
	{
	public:
		using base_iterator_t = Iterator;
		using value_type = decltype(*std::declval<Iterator>());
		using Out = decltype(std::declval<Loader>()(std::declval<value_type>()));

		using proxy_t = SelectWhere<Iterator, Filter, Loader>;
		using base_t = IState<proxy_t, base_iterator_t, Out, iterator_type::full>;
		using iterator_t = linq::iterator<proxy_t, base_iterator_t, Out, iterator_type::full>;

		typedef iterator_t iterator;
		typedef iterator_t const_iterator;

	private:
		friend iterator_t;
		Filter const _filter;
		Loader const _loader;

		inline bool validate(base_iterator_t const &it) const noexcept
		{
			return
				it != static_cast<base_iterator_t const &>(this->_end)
				&& !_filter(*it);
		}

		inline Out load(base_iterator_t const &it) const noexcept
		{
			return _loader(*it);
		}

	public:
		SelectWhere() = delete;
		~SelectWhere() = default;
		SelectWhere(SelectWhere const &) = default;
		SelectWhere(base_iterator_t const &begin, base_iterator_t const &end,
			Filter const &filter,
			Loader const &loader)
			: base_t(begin, end), _filter(filter), _loader(loader)
		{}

		template<typename Func>
		auto select(Func const &next_loader) const noexcept
		{
			const auto &last_loader = _loader;
			const auto new_loader = [last_loader, next_loader](value_type value) noexcept -> decltype(next_loader(std::declval<Out>()))
			{
				return next_loader(last_loader(value));
			};
			return SelectWhere<Iterator, Filter, decltype(new_loader)>(
				static_cast<base_iterator_t const &>(this->_begin),
				static_cast<base_iterator_t const &>(this->_end),
				_filter,
				new_loader);
		}

		template<typename Func>
		auto where(Func const &next_filter) const noexcept
		{
			const auto &last_loader = _loader;
			const auto &last_filter = _filter;
			const auto &new_filter = [last_loader, last_filter, next_filter](value_type value) noexcept -> bool
			{
				return last_filter(value) && next_filter(last_loader(value));
			};
			return SelectWhere<Iterator, decltype(new_filter), Loader>(
				std::find_if(static_cast<base_iterator_t const &>(this->_begin), static_cast<base_iterator_t const &>(this->_end), new_filter),
				static_cast<base_iterator_t const &>(this->_end),
				new_filter,
				last_loader);
		}

	};
	//Select
	template<typename Iterator, typename Loader>
	class Select
		: public IState
		<
		Select<Iterator, Loader>,
		Iterator,
		decltype(std::declval<Loader>()(std::declval<decltype(*std::declval<Iterator>())>())),
		iterator_type::load>
	{
	public:
		using base_iterator_t = Iterator;
		using value_type = decltype(*std::declval<Iterator>());
		using Out = decltype(std::declval<Loader>()(std::declval<value_type>()));

		using proxy_t = Select<Iterator, Loader>;
		using base_t = IState<proxy_t, base_iterator_t, Out, iterator_type::load>;
		using iterator_t = linq::iterator<proxy_t, base_iterator_t, Out, iterator_type::load>;

		typedef iterator_t iterator;
		typedef iterator_t const_iterator;
	private:
		friend iterator_t;
		Loader const _loader;

		inline Out load(base_iterator_t const &it) const noexcept
		{
			return _loader(*it);
		}
	public:
		Select() = delete;
		~Select() = default;
		Select(Select const &) = default;
		Select(base_iterator_t const &begin, base_iterator_t const &end,
			Loader const &loader)
			: base_t(begin, end), _loader(loader)
		{}

		template<typename Func>
		auto select(Func const &next_loader) const noexcept
		{
			const auto &last_loader = _loader;
			const auto &new_loader = [last_loader, next_loader](value_type value) noexcept -> decltype(next_loader(std::declval<Out>()))
			{
				return next_loader(last_loader(value));
			};
			return Select<Iterator, decltype(new_loader)>(
				static_cast<base_iterator_t const &>(this->_begin),
				static_cast<base_iterator_t const &>(this->_end),
				new_loader);
		}

		template<typename Func>
		auto where(Func const &next_filter) const noexcept
		{
			const auto &last_loader = _loader;
			const auto &new_filter = [last_loader, next_filter](value_type value) noexcept -> bool
			{
				return next_filter(last_loader(value));
			};
			return SelectWhere<Iterator, decltype(new_filter), Loader>(
				std::find_if(static_cast<base_iterator_t const &>(this->_begin), static_cast<base_iterator_t const &>(this->_end), new_filter),
				static_cast<base_iterator_t const &>(this->_end),
				new_filter,
				last_loader);
		}
	};
	//Where
	template<typename Iterator, typename Filter>
	class Where
		: public IState
		<
		Where<Iterator, Filter>,
		Iterator,
		decltype(*std::declval<Iterator>()),
		iterator_type::filter
		>
	{
	public:
		using base_iterator_t = Iterator;
		using value_type = decltype(*std::declval<Iterator>());
		using Out = decltype(*std::declval<Iterator>());

		using proxy_t = Where<Iterator, Filter>;
		using base_t = IState<proxy_t, base_iterator_t, Out, iterator_type::filter>;
		using iterator_t = linq::iterator<proxy_t, base_iterator_t, Out, iterator_type::filter>;

		typedef iterator_t iterator;
		typedef iterator_t const_iterator;

	private:
		friend iterator_t;
		Filter const _filter;

		inline bool validate(base_iterator_t const &it) const noexcept
		{
			return
				it != static_cast<base_iterator_t const &>(this->_end)
				&& !_filter(*it);
		}

	public:
		Where() = delete;
		~Where() = default;
		Where(Where const &) = default;
		Where(base_iterator_t const &begin, base_iterator_t const &end, Filter const &filter)
			: base_t(begin, end), _filter(filter)
		{}

		template<typename Func>
		auto select(Func const &next_loader) const noexcept
		{
			return SelectWhere<Iterator, Filter, decltype(next_loader)>(
				static_cast<base_iterator_t const &>(this->_begin),
				static_cast<base_iterator_t const &>(this->_end),
				_filter,
				next_loader);
		}

		template<typename Func>
		auto where(Func const &next_filter) const noexcept
		{
			const auto &last_filter = _filter;
			const auto &new_filter = [last_filter, next_filter](value_type value) noexcept -> bool
			{
				return last_filter(value) && next_filter(value);
			};
			return Where<Iterator, decltype(new_filter)>(
				std::find_if(static_cast<base_iterator_t const &>(this->_begin), static_cast<base_iterator_t const &>(this->_end), new_filter),
				static_cast<base_iterator_t const &>(this->_end),
				new_filter);
		}
	};
	//Take
	template <typename Iterator>
	class Take : public IState
		<
		Take<Iterator>,
		Iterator,
		decltype(*std::declval<Iterator>()),
		iterator_type::reach
		>
	{
	public:
		using Out = decltype(*std::declval<Iterator>());

		using proxy_t = Take<Iterator>;
		using base_t = IState
			<
			Take<Iterator>,
			Iterator,
			Out,
			iterator_type::reach
			>;
		using iterator_t = linq::iterator<proxy_t, Iterator, Out, iterator_type::reach>;

		typedef iterator_t iterator;
		typedef iterator_t const_iterator;
	private:
		friend iterator_t;

		mutable std::size_t number_;
		inline bool reach(Iterator const &) const { return number_ > 0; }
		inline void incr(Iterator const &) const { --number_; }

	public:
		Take() = delete;
		~Take() = default;
		Take(Take const &) = default;

		Take(Iterator const &begin, Iterator const &end, std::size_t number)
			: base_t(begin, end), number_(number)
		{}

		auto take(std::size_t number) const
		{
			return Take<Iterator>(this->_begin, this->_end, number);
		}
	};
	//From
	template<typename Iterator>
	class From
		: public IState
		<
		From<Iterator>,
		Iterator,
		decltype(*std::declval<Iterator>()),
		iterator_type::basic
		>
	{
	public:
		using base_iterator_t = Iterator;
		using value_type = decltype(*std::declval<Iterator>());


		using proxy_t = From<Iterator>;
		using base_t = IState<proxy_t, base_iterator_t, value_type, iterator_type::basic>;

		typedef Iterator iterator;
		typedef Iterator const_iterator;

	public:

		From() = delete;
		~From() = default;
		From(From const &) = default;
		From(base_iterator_t const &begin, base_iterator_t const &end)
			: base_t(begin, end)
		{}

	};
	//IState
	template<typename Proxy, typename Base_It, typename Out, iterator_type ItType>
	class IState
	{
		using iterator_t = iterator<Proxy, Base_It, Out, ItType>;
	protected:
		iterator_t const _begin;
		iterator_t const _end;
	public:
		IState() = delete;
		~IState() = default;
		IState(IState const &rhs)
			:
			_begin(rhs._begin, static_cast<Proxy const &>(*this)),
			_end(rhs._end, static_cast<Proxy const &>(*this))
		{}
		IState(Base_It const &begin, Base_It const &end)
			:
			_begin(begin, static_cast<Proxy const &>(*this)),
			_end(end, static_cast<Proxy const &>(*this))
		{}

		inline iterator_t const begin() const noexcept { return _begin; }
		inline iterator_t const end() const noexcept { return _end; }


		template<typename Func>
		auto select(Func const &next_loader) const noexcept
		{
			return Select<Base_It, Func>(
				static_cast<Base_It const &>(this->_begin),
				static_cast<Base_It const &>(this->_end),
				next_loader);
		}
		template<typename Func>
		auto where(Func const &next_filter) const noexcept
		{
			return Where<Base_It, Func>(
				std::find_if(static_cast<Base_It const &>(this->_begin), static_cast<Base_It const &>(this->_end), next_filter),
				static_cast<Base_It const &>(this->_end),
				next_filter);
		}
		template<typename... Funcs>
		auto groupBy(Funcs const &...keys) const noexcept
		{
			using group_type = group_by<Out, Funcs...>;
			using map_out = typename group_type::type;
			auto result = std::make_shared<map_out>();

			for (Out it : *this)
				group_type::emplace(*result, it, keys...);

			return GroupBy<typename map_out::iterator, decltype(result)>(result->begin(), result->end(), result);
		}
		template<typename... Funcs>
		auto orderBy(Funcs const &... keys) const noexcept
		{
			auto proxy = std::make_shared<std::vector<typename std::remove_reference<Out>::type>>();
			//std::copy(_begin, _end, std::back_inserter(*proxy));
			for (Out it : *this)
				proxy->push_back(it);
			std::sort(proxy->begin(), proxy->end(), [keys...](Out a, Out b) -> bool
			{
				return order_by(a, b, std::move(order_by_modifier_end<Out, Funcs>(keys))...);
			});

			return OrderBy<decltype(proxy->begin()), decltype(proxy)>(proxy->begin(), proxy->end(), proxy);
		}
		template<typename Func>
		auto skipWhile(Func const &func) const noexcept
		{
			auto ret = _begin;
			while (static_cast<Base_It const &>(ret) != static_cast<Base_It const &>(_end) && func(*ret))
				++ret;
			return From<iterator_t>(ret, _end);
		}
		//todo takeWhile
		auto skip(std::size_t offset) const noexcept
		{
			auto ret = _begin;
			for (std::size_t i = 0; i < offset && ret != _end; ++ret, ++i);

			return From<iterator_t>(ret, _end);
		}
		auto take(std::size_t number) const
		{
			return Take<iterator_t>(_begin, _end, number);
		}
		auto min() const noexcept
		{
			typename std::remove_const<typename std::remove_reference<decltype(*_begin)>::type>::type val(*_begin);
			for (Out it : *this)
				if (it > val)
					val = it;
			return val;
		}
		auto max() const noexcept
		{
			typename std::remove_const<typename std::remove_reference<decltype(*_begin)>::type>::type val(*_begin);
			for (Out it : *this)
				if (it < val)
					val = it;
			return val;
		}
		auto sum() const noexcept
		{
			typename std::remove_const<typename std::remove_reference<decltype(*_begin)>::type>::type result{};
			for (Out it : *this)
				result += it;
			return result;
		}
		auto count() const noexcept
		{
			std::size_t number{ 0 };
			for (auto it = _begin; it != _end; ++it, ++number);
			return number;
		}

		template<typename OutContainer>
		OutContainer to() const noexcept
		{
			OutContainer out;
			std::copy(_begin, _end, std::back_inserter(out));
			return std::move(out);
		}
		template<typename OutContainer>
		inline void to(OutContainer &out) const noexcept
		{
			std::copy(_begin, _end, std::back_inserter(out));
		}

	};

	template <typename Handle>
	class IEnumerable : public Handle
	{
		typedef typename Handle::iterator iterator_t;
		typedef decltype(*std::declval<iterator_t>()) out_t;

	public:
		IEnumerable() = delete;
		~IEnumerable() = default;
		IEnumerable(IEnumerable const &) = default;
		IEnumerable(Handle const &rhs)
			: Handle(rhs) {}

		template<typename Func>
		inline auto Select(Func const &next_loader) const noexcept
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).select(next_loader))>;
			return ret_t(std::move(static_cast<Handle const &>(*this).select(next_loader)));
		}
		template<typename Func>
		inline auto Where(Func const &next_filter) const noexcept
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).where(next_filter))>;
			return ret_t(std::move(static_cast<Handle const &>(*this).where(next_filter)));
		}
		template<typename... Funcs>
		inline auto GroupBy(Funcs const &...keys) const noexcept
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).groupBy(keys...))>;
			return ret_t(std::move(static_cast<Handle const &>(*this).groupBy(keys...)));
		}
		template<typename... Funcs>
		inline auto OrderBy(Funcs const &...keys) const noexcept
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).orderBy(keys...))>;
			return ret_t(std::move(static_cast<Handle const &>(*this).orderBy(keys...)));
		}
		template<typename Func>
		inline auto SkipWhile(Func const &func) const noexcept
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).skipWhile(func))>;
			return ret_t(std::move(static_cast<Handle const &>(*this).skipWhile(func)));
		}
		// TakeWhile todo

		inline auto Asc()
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).asc())>;
			return ret_t(std::move(static_cast<Handle const &>(*this).asc()));
		}
		inline auto Desc()
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).desc())>;
			return ret_t(std::move(static_cast<Handle const &>(*this).desc()));
		}

		inline auto Skip(std::size_t offset) const noexcept
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).skip(offset))>;
			return ret_t(std::move(static_cast<Handle const &>(*this).skip(offset)));
		}
		inline auto Take(std::size_t limit) const noexcept
		{
			using ret_t = IEnumerable<decltype(static_cast<Handle const &>(*this).take(limit))>;
			return ret_t(std::move(static_cast<Handle const &>(*this).take(limit)));
		}
		inline auto Min() const noexcept
		{
			return static_cast<Handle const &>(*this).min();
		}
		inline auto Max() const noexcept
		{
			return static_cast<Handle const &>(*this).max();
		}
		inline auto Sum() const noexcept
		{
			return static_cast<Handle const &>(*this).sum();
		}
		inline auto Count() const noexcept
		{
			return static_cast<Handle const &>(*this).count();
		}
		inline const iterator_t begin() const noexcept
		{
			return static_cast<Handle const &>(*this).begin();
		}
		inline const iterator_t end() const noexcept
		{
			return static_cast<Handle const &>(*this).end();
		}

		//template<typename OutProxy>
		//inline OutProxy To() const noexcept
		//{
		//	return std::move(static_cast<Handle &>(*this).to< OutProxy >());
		//}
		template<typename OutProxy>
		inline void To(OutProxy &out) const noexcept
		{
			return std::move(static_cast<Handle const &>(*this).to(out));
		}

		template<typename T>
		inline auto &operator[](T const &key) const
		{
			return static_cast<Handle const &>(*this).operator[](key);
		}

	};

	template<typename T>
	auto make_enumerable(T const &container)
	{
		return IEnumerable<From<typename T::const_iterator>>(From<typename T::const_iterator>(std::begin(container), std::end(container)));
	}
	template<typename T>
	auto make_enumerable(T &container)
	{
		return IEnumerable<From<typename T::iterator>>(From<typename T::iterator>(std::begin(container), std::end(container)));
	}
	template<typename T>
	auto make_enumerable(T const &begin, T const &end)
	{
		return IEnumerable<From<T>>(From<T>(begin, end));
	}

	template<typename T>
	auto from(T const &container)
	{
		return std::move(linq::From<typename T::const_iterator>(std::begin(container), std::end(container)));
	}
	template<typename T>
	auto from(T &container)
	{
		return std::move(linq::From<typename T::iterator>(std::begin(container), std::end(container)));
	}
	template<typename T>
	auto range(T const &begin, T const &end)
	{
		return std::move(linq::From<T>(begin, end));
	}

}

#endif // LINQ_H_INCLUDED
