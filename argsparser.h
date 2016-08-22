// Copyright (c) 2016 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdlib>

#ifdef _WIN32
#	include <io.h>
#	include <windows.h>
#else
#	include <sys/ioctl.h>
#	include <unistd.h>
#	define _isatty(FD) isatty(FD)
#	define _fileno(OBJ) fileno(OBJ)
#endif

#include "callable.h"

namespace args {
	class parser;

	namespace actions {
		struct action {
			virtual ~action() {}
			virtual bool required() const = 0;
			virtual void required(bool value) = 0;
			virtual bool multiple() const = 0;
			virtual void multiple(bool value) = 0;
			virtual bool needs_arg() const = 0;
			virtual void visit(parser&) = 0;
			virtual void visit(parser&, const std::string& /*arg*/) = 0;
			virtual bool visited() const = 0;
			virtual void meta(const std::string& s) = 0;
			virtual const std::string& meta() const = 0;
			virtual const char* c_meta() const = 0;
			virtual void help(const std::string& s) = 0;
			virtual const std::string& help() const = 0;
			virtual bool is(const std::string& name) const = 0;
			virtual bool is(char name) const = 0;
			virtual const std::vector<std::string>& names() const = 0;

			void append_short_help(std::string& s) const
			{
				std::string aname;
				if (names().empty()) {
					aname = c_meta();
				} else {
					aname.push_back('-');
					auto& name = names().front();
					if (name.length() != 1)
						aname.push_back('-');
					aname.append(name);

					if (needs_arg()) {
						aname.push_back(' ');
						aname.append(c_meta());
					}
				}

				int flags =
					(required() ? 2 : 0) |
					(multiple() ? 1 : 0);

				if (flags & 2) {
					s.push_back(' ');
					s.append(aname);
				}

				if (flags & 1) {
					s.append(" [");
					s.append(aname);
					s.append(" ...]");
				}

				if (!flags) {
					s.append(" [");
					s.append(aname);
					s.push_back(']');
				}
			}

			std::string help_name() const
			{
				std::string nmz;
				bool first = true;
				for (auto& name : names()) {
					if (first) first = false;
					else nmz.append(", ");

					if (nmz.length() == 1)
						nmz.append("-");
					else
						nmz.append("--");
					nmz.append(name);
				}

				if (nmz.empty())
					return c_meta();

				if (needs_arg()) {
					nmz.push_back(' ');
					nmz.append(c_meta());
				}

				return nmz;
			}
		};

		class action_base : public action {
			bool visited_ = false;
			bool required_ = true;
			bool multiple_ = false;
			std::vector<std::string> names_;
			std::string meta_;
			std::string help_;

			static void pack(std::vector<std::string>&) {}
			template <typename Name, typename... Names>
			static void pack(std::vector<std::string>& dst, Name&& name, Names&&... names)
			{
				dst.emplace_back(std::forward<Name>(name));
				pack(dst, std::forward<Names>(names)...);
			}

		protected:
			template <typename... Names>
			action_base(Names&&... argnames)
			{
				pack(names_, std::forward<Names>(argnames)...);
			}

			void visited(bool val) { visited_ = val; }
		public:
			void required(bool value) override { required_ = value; }
			bool required() const override { return required_; }
			void multiple(bool value) override { multiple_ = value; }
			bool multiple() const override { return multiple_; }

			void visit(parser&) override { visited_ = true; }
			void visit(parser&, const std::string& /*arg*/) override { visited_ = true; }
			bool visited() const override { return visited_; }
			void meta(const std::string& s) override { meta_ = s; }
			const std::string& meta() const override { return meta_; }
			const char* c_meta() const override { return meta_.empty() ? "ARG" : meta_.c_str(); }
			void help(const std::string& s) override { help_ = s; }
			const std::string& help() const override { return help_; }

			bool is(const std::string& name) const override
			{
				for (auto& argname : names_) {
					if (argname.length() > 1 && argname == name)
						return true;
				}

				return false;
			}

			bool is(char name) const override
			{
				for (auto& argname : names_) {
					if (argname.length() == 1 && argname[0] == name)
						return true;
				}

				return false;
			}

			const std::vector<std::string>& names() const override
			{
				return names_;
			}
		};

		class builder {
			friend class parser;

			actions::action* ptr;
			builder(actions::action* ptr) : ptr(ptr) { }
			builder(const builder&);
		public:
			builder& meta(const std::string& name)
			{
				ptr->meta(name);
				return *this;
			}
			builder& help(const std::string& dscr)
			{
				ptr->help(dscr);
				return *this;
			}
			builder& multi(bool value = true)
			{
				ptr->multiple(value);
				return *this;
			}
			builder& req(bool value = true)
			{
				ptr->required(value);
				return *this;
			}
			builder& opt(bool value = true)
			{
				ptr->required(!value);
				return *this;
			}
		};

		template <typename T>
		class store_action : public action_base {
			T* ptr;
		public:
			template <typename... Names>
			explicit store_action(T* dst, Names&&... names) : action_base(std::forward<Names>(names)...), ptr(dst) {}

			bool needs_arg() const override { return true; }
			void visit(parser&, const std::string& arg) override
			{
				*ptr = arg;
				visited(true);
			}
		};

		template <typename T>
		class store_action<std::vector<T>> final : public action_base {
			std::vector<T>* ptr;
		public:
			template <typename... Names>
			explicit store_action(std::vector<T>* dst, Names&&... names) : action_base(std::forward<Names>(names)...), ptr(dst)
			{
				action_base::multiple(true);
			}

			bool needs_arg() const override { return true; }
			void visit(parser&, const std::string& arg) override
			{
				ptr->push_back(arg);
				visited(true);
			}
		};

		template <typename T, typename Value>
		class value_action : public action_base {
			T* ptr;
		public:
			template <typename... Names>
			explicit value_action(T* dst, Names&&... names) : action_base(std::forward<Names>(names)...), ptr(dst)
			{
			}

			bool needs_arg() const override { return false; }
			void visit(parser&) override
			{
				*ptr = Value::value;
				visited(true);
			}
		};

		namespace detail {
			template <typename Callable, bool Forward, typename... Args>
			class custom_adapter_impl {
			public:
				custom_adapter_impl(Callable cb) : cb(std::move(cb))
				{
				}
				void operator()(parser& p, Args&&... args)
				{
					cb(p, std::forward<Args>(args)...);
				}
			private:
				Callable cb;
			};

			template <typename Callable, typename... Args>
			class custom_adapter_impl<Callable, false, Args...> {
			public:
				custom_adapter_impl(Callable cb) : cb(std::move(cb))
				{
				}
				void operator()(parser&, Args&&... args)
				{
					cb(std::forward<Args>(args)...);
				}
			private:
				Callable cb;
			};

			template <typename Callable, typename... Args>
			using custom_adapter = custom_adapter_impl<
				Callable,
				stdex::is_callable<void(parser&, Args...), Callable>::value,
				Args...>;
		}

		template <typename Callable, typename Enable = void> class custom_action;

		template <typename Callable>
		class custom_action<Callable, std::enable_if_t<stdex::is_callable_or<Callable, void(), void(parser&)>::value>> : public action_base {
			detail::custom_adapter<Callable> cb;
		public:
			template <typename... Names>
			explicit custom_action(Callable cb, Names&&... names) : action_base(std::forward<Names>(names)...), cb(std::move(cb)) {}

			bool needs_arg() const override { return false; }
			void visit(parser& p) override
			{
				cb(p);
				visited(true);
			}
		};

		template <typename Callable>
		class custom_action<Callable, std::enable_if_t<stdex::is_callable_or<Callable, void(const std::string&), void(parser&, const std::string&)>::value>> : public action_base {
			detail::custom_adapter<Callable> cb;
		public:
			template <typename... Names>
			explicit custom_action(Callable cb, Names&&... names) : action_base(std::forward<Names>(names)...), cb(std::move(cb)) {}

			bool needs_arg() const override { return true; }
			void visit(parser& p, const std::string& s) override
			{
				cb(p, s);
				visited(true);
			}
		};
	}

	namespace detail {
		inline bool is_terminal(FILE* out)
		{
			return !!_isatty(_fileno(out));
		}

		inline int terminal_width(FILE* out)
		{
#ifdef _WIN32
			CONSOLE_SCREEN_BUFFER_INFO buffer = { sizeof(CONSOLE_SCREEN_BUFFER_INFO) };
			auto handle = (HANDLE)_get_osfhandle(_fileno(out));
			if (handle == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(handle, &buffer))
				return 0;
			return buffer.dwSize.X;
#else
			winsize w;
			while (ioctl(fileno(out), TIOCGWINSZ, &w) == -1) {
				if (errno != EINTR)
					return 0;
			}
			return w.ws_col;
#endif
		}

		template <typename It>
		inline It split(It cur, It end, size_t width)
		{
			if (size_t(end - cur) <= width)
				return end;

			auto c_end = cur + width;
			auto it = cur;
			while (true) {
				auto prev = it;
				while (it != c_end && *it == ' ') ++it;
				while (it != c_end && *it != ' ') ++it;
				if (it == c_end) {
					if (prev == cur || *c_end == ' ')
						return c_end;
					return prev;
				}
			}
		}

		template <typename It>
		inline It skip_ws(It cur, It end)
		{
			while (cur != end && *cur == ' ') ++cur;
			return cur;
		}
	}

	struct chunk {
		std::string title;
		std::vector<std::pair<std::string, std::string>> items;
	};

	using fmt_list = std::vector<chunk>;

	struct file_printer {
		file_printer(FILE* out) : out(out)
		{
		}
		void print(const char* cur, size_t len)
		{
			if (len)
				fwrite(cur, 1, len, out);
		}
		void putc(char c)
		{
			fputc(c, out);
		}
		size_t width()
		{
			if (detail::is_terminal(out))
				return detail::terminal_width(out);
			return 0;
		}
	private:
		FILE* out;
	};

	template <typename output>
	struct printer_base_impl : output {
		using output::output;
		inline void format_paragraph(const std::string& text, size_t indent, size_t width)
		{
			if (width < 2)
				width = text.length();
			else
				--width;

			if (indent >= width)
				indent = 0;

			auto cur = text.begin();
			auto end = text.end();
			auto chunk = detail::split(cur, end, width);

			output::print(&*cur, chunk - cur);
			output::putc('\n');

			cur = detail::skip_ws(chunk, end);
			if (cur == end)
				return;

			std::string pre(indent, ' ');
			width -= (int)indent;

			while (cur != end) {
				chunk = detail::split(cur, end, width);
				output::print(pre.c_str(), pre.length());
				output::print(&*cur, chunk - cur);
				output::putc('\n');
				cur = detail::skip_ws(chunk, end);
			}
		}
		inline void format_list(const fmt_list& info, size_t width)
		{
			size_t len = 0;
			for (auto& chunk : info) {
				for (auto& pair : chunk.items) {
					if (len < std::get<0>(pair).length())
						len = std::get<0>(pair).length();
				}
			}

			if (width < 20) { // magic treshold
				for (auto& chunk : info) {
					output::putc('\n');
					output::print(chunk.title.c_str(), chunk.title.length());
					output::print(":\n", 2);
					for (auto& pair : chunk.items) {
						output::putc(' ');
						output::print(std::get<0>(pair).c_str(), std::get<0>(pair).length());
						for (size_t i = 0, max = len - std::get<0>(pair).length() + 1; i < max; ++i)
							output::putc(' ');
						output::print(std::get<1>(pair).c_str(), std::get<1>(pair).length());
						output::putc('\n');
					}
				}

				return;
			}

			auto proposed = width / 3;
			len += 2;
			if (len > proposed)
				len = proposed;
			len -= 2;

			for (auto& chunk : info) {
				output::putc('\n');
				format_paragraph(chunk.title + ":", 0, width);
				for (auto& pair : chunk.items) {
					auto prefix = (len < std::get<0>(pair).length() ? std::get<0>(pair).length() : len) + 2;

					std::string sum;
					sum.reserve(prefix + std::get<1>(pair).length());
					sum.push_back(' ');
					sum.append(std::get<0>(pair));
					// prefix - (initial space + the value for the first column)
					sum.append(prefix - 1 - std::get<0>(pair).length(), ' ');
					sum.append(std::get<1>(pair));
					format_paragraph(sum, prefix, width);
				}
			}
		}
	};

	template <typename output>
	struct printer_base : printer_base_impl<output> {
		using printer_base_impl::printer_base_impl;
	};

	template <>
	struct printer_base<file_printer> : printer_base_impl<file_printer> {
		using printer_base_impl::printer_base_impl;
		using printer_base_impl::format_paragraph;
		using printer_base_impl::format_list;
		inline void format_paragraph(const std::string& text, size_t indent)
		{
			format_paragraph(text, indent, width());
		}
		inline void format_list(const fmt_list& info)
		{
			format_list(info, width());
		}
	};

	using printer = printer_base<file_printer>;

	class parser {
		std::vector<std::unique_ptr<actions::action>> actions_;
		std::string description_;
		std::vector<const char*> args_;
		std::string prog_;
		std::string usage_;
		bool provide_help_ = true;

#ifdef _WIN32
		static constexpr char DIRSEP = '\\';
#else
		static constexpr char DIRSEP = '/';
#endif
		std::string program_name(const char* arg0)
		{
			auto program = std::strrchr(arg0, DIRSEP);
			if (program) ++program;
			else program = arg0;

#ifdef WIN32
			auto ext = std::strrchr(program, '.');
			if (ext && ext != program)
				return { program, ext };
#endif

			return program;
		}

		std::pair<size_t, size_t> count_args() const
		{
			size_t positionals = 0;
			size_t arguments = provide_help_ ? 1 : 0;

			for (auto& action : actions_) {
				if (action->names().empty())
					++positionals;
				else
					++arguments;
			}

			return { positionals, arguments };
		}

		static chunk& make_title(chunk& part, std::string title, size_t count)
		{
			part.title = std::move(title);
			part.items.reserve(count);
			return part;
		}

		static std::string expand(char c)
		{
			char buff[] = { c, 0 };
			return buff;
		}

		void parse_long(const std::string& name, size_t& i)
		{
			if (provide_help_ && name == "help")
				help();

			for (auto& action : actions_) {
				if (!action->is(name))
					continue;

				if (action->needs_arg()) {
					++i;
					if (i >= args_.size())
						error("argument --" + name + ": expected one argument");

					action->visit(*this, args_[i]);
				} else
					action->visit(*this);

				return;
			}

			error("unrecognized argument: --" + name);
		}

		void parse_short(const std::string& name, size_t& arg)
		{
			auto length = name.length();
			for (decltype(length) i = 0; i < length; ++i) {
				auto c = name[i];
				if (provide_help_ && c == 'h')
					help();

				bool found = false;
				for (auto& action : actions_) {
					if (!action->is(c))
						continue;

					if (action->needs_arg()) {
						std::string param;

						++i;
						if (i < length)
							param = name.substr(i);
						else {
							++arg;
							if (arg >= args_.size())
								error("argument -" + expand(c) + ": expected one argument");

							param = args_[arg];
						}

						i = length;

						action->visit(*this, param);
					} else
						action->visit(*this);

					found = true;
					break;
				}

				if (!found)
					error("unrecognized argument: -" + expand(c));
			}
		}

		void parse_positional(const std::string& value)
		{
			for (auto& action : actions_) {
				if (!action->names().empty())
					continue;

				action->visit(*this, value);
				return;
			}

			error("unrecognized argument: " + value);
		}

		template <typename T, typename... Args>
		actions::action* add(Args&&... args)
		{
			actions_.push_back(std::make_unique<T>(std::forward<Args>(args)...));
			return actions_.back().get();
		}
	public:
		parser(const std::string& description, int argc, char* argv[]) : description_(description), prog_{ program_name(argv[0]) }
		{
			args_.reserve(argc - 1);
			for (int i = 1; i < argc; ++i)
				args_.emplace_back(argv[i]);
		}

		template <typename T, typename... Names>
		actions::builder arg(T& dst, Names&&... names) {
			return add<actions::store_action<T>>(&dst, std::forward<Names>(names)...);
		}

		template <typename Value, typename T, typename... Names>
		actions::builder set(T& dst, Names&&... names) {
			return add<actions::value_action<T, Value>>(&dst, std::forward<Names>(names)...);
		}

		template <typename Callable, typename... Names>
		std::enable_if_t<stdex::is_callable_or<Callable,
			void(),
			void(parser&),
			void(const std::string&),
			void(parser&, const std::string&)
		>::value, actions::builder> custom(Callable cb, Names&&... names)
		{
			return add<actions::custom_action<Callable>>(std::move(cb), std::forward<Names>(names)...);
		}

		void program(const std::string& value) { prog_ = value; }
		const std::string& program() { return prog_; }

		void usage(const std::string& value) { usage_ = value; }
		const std::string& usage() { return usage_; }

		void provide_help(bool value = true) { provide_help_ = value; }
		bool provide_help() { return provide_help_; }

		const std::vector<const char*>& args() const { return args_; }

		void parse()
		{
			auto count = args_.size();
			for (decltype(count) i = 0; i < count; ++i) {
				auto& arg = args_[i];
				auto length = arg ? strlen(arg) : 0;
				if (length > 1 && arg[0] == '-') {
					if (length > 2 && arg[1] == '-')
						parse_long(arg + 2, i);
					else
						parse_short(arg + 1, i);
				} else
					parse_positional(arg);
			}

			for (auto& action : actions_) {
				if (action->required() && !action->visited()) {
					std::string arg;
					if (action->names().empty()) {
						arg = action->c_meta();
					} else {
						auto& name = action->names().front();
						arg = name.length() == 1 ? "-" + name : "--" + name;
					}
					error("argument " + arg + " is required");
				}
			}
		}

		void short_help(FILE* out = stdout)
		{
			std::string shrt { "usage: " };
			shrt.append(prog_);

			if (!usage_.empty()) {
				shrt.push_back(' ');
				shrt.append(usage_);
			} else {
				if (provide_help_)
					shrt.append(" [-h]");

				for (auto& action : actions_)
					action->append_short_help(shrt);
			}

			printer{ out }.format_paragraph(shrt, 7);
		}

		[[noreturn]] void help()
		{
			short_help();

			if (!description_.empty()) {
				fputc('\n', stdout);
				printer { stdout }.format_paragraph(description_, 0);
			}

			size_t positionals = 0;
			size_t arguments = 0;
			std::tie(positionals, arguments) = count_args();

			fmt_list info(positionals ? arguments ? 2 : 1 : arguments ? 1 : 0);

			size_t args_id = 0;
			if (positionals)
				make_title(info[args_id++], "positional arguments", positionals);

			if (arguments) {
				auto& args = make_title(info[args_id], "optional arguments", arguments);
				if (provide_help_)
					args.items.push_back(std::make_pair("-h, --help", "show this help message and exit"));
			}

			for (auto& action : actions_) {
				info[action->names().empty() ? 0 : args_id].items
					.push_back(std::make_pair(action->help_name(), action->help()));
			}

			printer { stdout }.format_list(info);

			std::exit(0);
		}

		[[noreturn]] void error(const std::string& msg)
		{
			short_help(stderr);
			printer { stderr }.format_paragraph(prog_ + ": error: " + msg, 0);
			std::exit(2);
		}
	};
}
