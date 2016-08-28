// Copyright (c) 2016 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once
#include <type_traits>

namespace stdex {
	template <typename Prototype, typename Callable> struct is_callable : std::false_type {
	};

	template <typename Return, typename... Args, typename Callable>
	struct is_callable<Return(Args...), Callable> {
	private:
		typedef char(&yes)[1];
		typedef char(&no)[2];

		template <typename C, typename... A>
		using result_of = decltype(std::declval<C>()(std::declval<A>()...));

		template<typename C> static yes test(result_of<C, Args...>*);
		template<typename C> static no  test(...);

		template<bool HasArgs, typename C, typename R, typename... A> struct return_helper:
			std::is_convertible<result_of<C, A...>, R> { };
		template<typename C, typename... A> struct return_helper<true, C, void, A...> : std::true_type { };
		template<typename C, typename R, typename... A> struct return_helper<false, C, R, A...> : std::false_type { };

		static constexpr bool value_args = sizeof(test<Callable>(nullptr)) == sizeof(yes);
		static constexpr bool value_return = return_helper<value_args, Callable, Return, Args...>::value;
	public:
		static constexpr bool value = value_args && value_return;
	};

	template<typename... B>
	struct or_;

	template<>
	struct or_<> : std::false_type {
	};

	template<typename B>
	struct or_<B> : B {
	};

	template <bool Value, typename Head, typename... Tail>
	struct select_or_ : Head {
	};

	template <typename Head, typename... Tail>
	struct select_or_<false, Head, Tail...> : or_<Tail...> {
	};

	template<typename Head, typename... Tail>
	struct or_<Head, Tail...> : select_or_<Head::value, Head, Tail...> {
	};

	template <typename Callable, typename... Prototypes>
	struct is_callable_or : or_<is_callable<Prototypes, Callable>...> {
	};
}

