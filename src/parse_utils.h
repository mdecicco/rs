#pragma once
#include <defs.h>
#include <string>
#include <stack>
#include <vector>

namespace rs {
	std::vector<std::string> split(const std::string& str, const std::string& delimiters);
	std::string format(const char* fmt, ...);

	class parse_exception : public std::exception {
		public:
			parse_exception(const std::string& text, const std::string& file, const std::string& lineText, u32 line, u32 col);
			~parse_exception();

			virtual const char* what() { return text.c_str(); }
			std::string text;
			std::string file;
			std::string lineText;
			u32 line;
			u32 col;
	};

	class tokenizer {
		public:
			tokenizer(const std::string& file, const std::string& input);
			~tokenizer();

			struct token {
				u32 line = 0;
				u32 col = 0;
				std::string text = "";
				std::string file = "";

				inline const bool valid() const { return text.length() != 0; }
				inline const bool operator == (const token& rhs) const { return text == rhs.text; }
				inline const bool operator == (const std::string& rhs) const { return text == rhs; }
			};

			void specify_keyword(const std::string& keyword);
			bool is_keyword(const std::string& thing);
			inline u32 line() const { return m_line; }
			inline u32 col() const { return m_col; }
			inline const bool at_end(bool check_whitespace = true) {
				if (check_whitespace) whitespace();
				return m_idx == m_input.length();
			}

			void backup_state();
			void commit_state();
			void restore_state();

			bool whitespace();
			std::string thing();
			token semicolon(bool expected = true);
			token keyword(bool expected = true, const std::string& kw = "");
			token identifier(bool expected = true, const std::string& identifier = "");
			token character(char c, bool expected = true);
			token string_constant(bool expected = true, bool strip_quotes = false, bool allow_empty = true);
			token number_constant(bool expected = true);

			std::vector<std::string> lines;

		protected:
			std::vector<std::string> m_keywords;
			std::string m_file;
			std::string m_input;
			struct stored_state {
				u32 idx;
				u32 line;
				u32 col;
			};
			std::stack<stored_state> m_storedState;
			u32 m_idx;
			u32 m_line;
			u32 m_col;
	};
};