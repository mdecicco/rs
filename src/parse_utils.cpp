#include <parse_utils.h>
#include <stdarg.h>
using namespace std;

namespace rs {
	vector<string> split(const string& str, const string& delimiters) {
		vector<string> out;

		string cur;
		bool lastWasDelim = false;
		for(size_t i = 0;i < str.length();i++) {
			bool isDelim = false;
			for(unsigned char d = 0;d < delimiters.length() && !isDelim;d++) {
				isDelim = str[i] == delimiters[d];
			}

			if (isDelim) {
				if (!lastWasDelim) {
					out.push_back(cur);
					cur = "";
					lastWasDelim = true;
				}
				continue;
			}

			cur += str[i];
			lastWasDelim = false;
		}

		if (cur.length() > 0) out.push_back(cur);
		return out;
	}

	string format(const char* fmt, ...) {
		va_list l;
		va_start(l, fmt);
		char out[1024] = { 0 };
		vsnprintf(out, 1024, fmt, l);
		va_end(l);
		return out;
	}

	parse_exception::parse_exception(const string& _text, const string& _file, const string& _lineText, u32 _line, u32 _col) {
		text = _text;
		file = _file;
		lineText = _lineText;
		line = _line;
		col = _col;
	}

	parse_exception::~parse_exception() { }

	tokenizer::tokenizer(const string& file, const string& input) {
		m_input = input;
		m_file = file;
		m_line = 0;
		m_col = 0;
		m_idx = 0;

		lines = split(input, "\n\r");
	}

	tokenizer::~tokenizer() {
	}

	void tokenizer::specify_keyword(const string& keyword) {
		m_keywords.push_back(keyword);
	}
	
	bool tokenizer::is_keyword(const string& thing) {
		for (const string& kw : m_keywords) {
			if (kw == thing) return true;
		}

		return false;
	}

	void tokenizer::backup_state() {
		m_storedState.push({ m_idx, m_line, m_col });
	}

	void tokenizer::commit_state() {
		m_storedState.pop();
	}

	void tokenizer::restore_state() {
		auto s = m_storedState.top();
		m_storedState.pop();
		m_idx = s.idx;
		m_line = s.line;
		m_col = s.col;
	}

	bool tokenizer::whitespace() {
		bool lastWasNewline = false;
		bool hadWhitespace = false;
		while (!at_end(false)) {
			char c = m_input[m_idx];
			if (c == ' ' || c == '\t') {
				lastWasNewline = false;
				hadWhitespace = true;
				m_col++;
				m_idx++;
				continue;
			}

			if (c == '\n' || c == '\r') {
				if (!lastWasNewline) {
					lastWasNewline = true;
					if (m_line < lines.size() - 1) {
						m_line++;
						m_col = 0;
					}
				}
				m_idx++;
				continue;
			}

			break;
		}

		return hadWhitespace;
	}

	string tokenizer::thing() {
		string out;
		while(!at_end()) {
			if (whitespace()) break;
			out += m_input[m_idx++];
		}

		return out;
	}

	tokenizer::token tokenizer::semicolon(bool expected) {
		return character(';', expected);
	}

	tokenizer::token tokenizer::keyword(bool expected, const string& kw) {
		token out;

		whitespace();
		out.line = m_line;
		out.col = m_col;
		out.file = m_file;

		size_t offset = 0;

		while(!at_end()) {
			char c = m_input[m_idx + offset];
			if (c >= 48 && c <= 57 && out.text.length() == 0) {
				if (!expected) return out;
				// 0 - 9
				throw parse_exception(
					"Expected keyword, found numerical constant",
					m_file,
					lines[m_line],
					m_line,
					m_col
				);
			}

			if (c == '\'' || c == '"' && out.text.length() == 0) {
				if (!expected) return out;
				throw parse_exception(
					"Expected keyword, found string constant",
					m_file,
					lines[m_line],
					m_line,
					m_col
				);
			}

			if (
				(c >= 48 && c <= 57 ) || // 0 - 9
				(c >= 65 && c <= 90 ) || // A - Z
				(c >= 97 && c <= 122) || // a - z
				c == '_' || c == '+' || c == '-' ||
				c == '*' || c == '/' || c == '&' ||
				c == '%' || c == '^' || c == '|' ||
				c == '=' || c == '!' || c == '<' ||
				c == '>'
			) {
				out.text += c;
				offset++;
			} else {
				if (out.text.length() == 0) {
					if (!expected) return out;
					throw parse_exception(
						"Expected keyword, found something else",
						m_file,
						lines[m_line],
						m_line,
						m_col
					);
				} else break;
			}
		}

		if (!is_keyword(out.text)) {
			if (!expected) return { 0, 0, "", "" };
			throw parse_exception(
				"Expected keyword, found identifier",
				m_file,
				lines[m_line],
				m_line,
				m_col
			);
		}

		if (out.text != kw && kw.length() > 0) {
			if (!expected) return { 0, 0, "", "" };
			throw parse_exception(
				format("Expected keyword '%s', found '%s'", kw.c_str(), out.text.c_str()),
				m_file,
				lines[m_line],
				m_line,
				m_col
			);
		}

		m_idx += offset;
		m_col += offset;

		return out;
	}

	tokenizer::token tokenizer::identifier(bool expected, const string& identifier) {
		token out;

		whitespace();
		out.line = m_line;
		out.col = m_col;
		out.file = m_file;

		size_t offset = 0;

		while(!at_end()) {
			char c = m_input[m_idx + offset];
			if (c >= 48 && c <= 57 && out.text.length() == 0) {
				if (!expected) return out;
				// 0 - 9
				throw parse_exception(
					"Expected identifier, found numerical constant",
					m_file,
					lines[m_line],
					m_line,
					m_col
				);
			}

			if (c == '\'' || c == '"' && out.text.length() == 0) {
				if (!expected) return out;
				throw parse_exception(
					"Expected identifier, found string constant",
					m_file,
					lines[m_line],
					m_line,
					m_col
				);
			}

			if (
				(c >= 48 && c <= 57 ) || // 0 - 9
				(c >= 65 && c <= 90 ) || // A - Z
				(c >= 97 && c <= 122) || // a - z
				c == '_'
			) {
				out.text += c;
				offset++;
			} else {
				if (out.text.length() == 0) {
					if (!expected) return out;
					throw parse_exception(
						"Expected identifier, found something else",
						m_file,
						lines[m_line],
						m_line,
						m_col
					);
				} else break;
			}
		}
		
		if (is_keyword(out.text)) {
			if (!expected) return { 0, 0, "", "" };
			throw parse_exception(
				"Expected identifier, found keyword",
				m_file,
				lines[m_line],
				m_line,
				m_col
			);
		}

		if (out.text != identifier && identifier.length() > 0) {
			if (!expected) return { 0, 0, "", "" };
			throw parse_exception(
				format("Expected identifier '%s', found '%s'", identifier.c_str(), out.text.c_str()),
				m_file,
				lines[m_line],
				m_line,
				m_col
			);
		}

		m_idx += offset;
		m_col += offset;

		return out;
	}

	tokenizer::token tokenizer::character(char c, bool expected) {
		token out;

		whitespace();
		out.line = m_line;
		out.col = m_col;
		out.file = m_file;

		if (m_input[m_idx] == c) {
			out.text = c;
			m_idx++;
			m_col++;
		} else {
			if (!expected) return out;
			string found = thing();
			throw parse_exception(
				format("Expected '%c', found \"%s\"", c, found.c_str()),
				m_file,
				lines[m_line],
				m_line,
				m_col
			);
		}

		return out;
	}

	tokenizer::token tokenizer::string_constant(bool expected) {
		token out;

		whitespace();
		out.line = m_line;
		out.col = m_col;
		out.file = m_file;

		token bt = character('\'', expected);
		if (bt.text.length() == 0) return { 0, 0, "", "" };

		out.text += '\'';
		bool foundEnd = false;
		bool lastWasNewline = false;
		while(!at_end()) {
			char last = 0;
			if (out.text.length() > 0) {
				last = out.text[out.text.length() - 1];
			}

			token end = character('\'', false);
			if (end.text.length() != 0 && last != '\\') {
				out.text += '\'';
				foundEnd = true;
				break;
			}

			char c = m_input[m_idx++];
			out.text += c;
			m_col++;

			if (c == '\n' || c == '\r') {
				if (!lastWasNewline) {
					lastWasNewline = true;
					m_line++;
					m_col = 0;
				}
			} else lastWasNewline = false;
		}

		if (!foundEnd) {
			throw parse_exception(
				"Encountered unexpected end of file while parsing string constant",
				m_file,
				lines[m_line],
				m_line,
				m_col
			);
		}

		return out;
	}

	tokenizer::token tokenizer::number_constant(bool expected) {
		token out;

		whitespace();
		out.line = m_line;
		out.col = m_col;
		out.file = m_file;

		bool isNeg = false;
		bool hasDecimal = false;
		size_t offset = 0;

		while (!at_end()) {
			char c = m_input[m_idx + offset];
			if (c == '-') {
				if (out.text.length() != 0) break;
				isNeg = true;
				out.text += c;
				offset++;
			} else if (c == '.') {
				if (hasDecimal) {
					if (!expected) return { 0, 0, "", "" };
					throw parse_exception(
						"Numerical constants can only contain one decimal point",
						m_file,
						lines[m_line],
						m_line,
						m_col
					);
				}
				hasDecimal = true;
				out.text += c;
				offset++;
			} else if (c >= 48 && c <= 57) {
				out.text += c;
				offset++;
			} else {
				if (out.text.length() == 0) {
					if (!expected) return { 0, 0, "", "" };
					throw parse_exception(
						format("Expected numerical constant, found '%s'", thing().c_str()),
						m_file,
						lines[m_line],
						m_line,
						m_col
					);
				}
				break;
			}
		}

		m_idx += offset;
		m_col += offset;

		return out;
	}
};