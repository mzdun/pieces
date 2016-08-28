// Copyright (c) 2016 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <type_traits>

namespace fmt {
	template <typename T> struct str_of;

	template <> struct str_of<std::string> {
		static std::string get(const std::string& s) { return s; }
	};

	template <> struct str_of<const char*> {
		static std::string get(const char* s) { return s; }
	};

	template <typename I> struct str_of_std {
		static std::string get(I i) { return std::to_string(i); }
	};

	template <> struct str_of<int> : str_of_std<int> {};
	template <> struct str_of<long> : str_of_std<long> {};
	template <> struct str_of<long long> : str_of_std<long long> {};
	template <> struct str_of<unsigned> : str_of_std<unsigned> {};
	template <> struct str_of<unsigned long> : str_of_std<unsigned long> {};
	template <> struct str_of<unsigned long long> : str_of_std<unsigned long long> {};
	template <> struct str_of<float> : str_of_std<float> {};
	template <> struct str_of<double> : str_of_std<double> {};
	template <> struct str_of<long double> : str_of_std<long double> {};

	namespace detail {
		template <size_t Index, typename Tuple>
		std::string str_of_one(const Tuple& args)
		{
			using raw_arg = decltype(std::get<Index>(args));
			using arg_t = std::decay_t<raw_arg>;
			return str_of<arg_t>::get(std::get<Index>(args));
		}

		template <typename Indexes>
		struct str_of_select;

		template <>
		struct str_of_select<std::index_sequence<>> {
			template <typename Tuple>
			static std::string get(size_t, const Tuple&)
			{
				return { };
			}
		};

		template <size_t Head, size_t... Tail>
		struct str_of_select<std::index_sequence<Head, Tail...>> {
			template <typename Tuple>
			static std::string get(size_t i, const Tuple& args)
			{
				if (i == Head)
					return str_of_one<Head>(args);

				return str_of_select<std::index_sequence<Tail...>>::get(i, args);
			}
		};

		template <typename It, typename... Args>
		std::string str(It from, It to, const std::tuple<Args...>& args)
		{
			using str = str_of_select<std::make_index_sequence<sizeof...(Args)>>;
			std::string out;
			while (from != to) {
				if (*from == '$') {
					size_t ndx = 0;
					++from;
					while (from != to) {
						bool do_break = false;

						switch (*from) {
						case '0': case '1': case '2': case '3': case '4':
						case '5': case '6': case '7': case '8': case '9':
							ndx *= 10;
							ndx += *from++ - '0';
							break;
						default:
							do_break = true;
						};

						if (do_break)
							break;
					}
					if (ndx != 0)
						out.append(str::get(--ndx, args));

					continue;
				}
				out.push_back(*from++);
			}
			return out;
		}

		template <typename It>
		std::string str(It from, It end, const std::tuple<>& args)
		{
			return { from, end };
		}
	}

	template <typename... Args>
	std::string str(const char* ptr, Args&&... args)
	{
		if (!ptr || !*ptr)
			return { };
		return detail::str(ptr, ptr + strlen(ptr), std::make_tuple(std::forward<Args>(args)...));
	}

	template <typename... Args>
	std::string str(const std::string& s, Args&&... args)
	{
		if (s.empty())
			return { };
		return detail::str(s.begin(), s.end(), std::make_tuple(std::forward<Args>(args)...));
	}
}
