#pragma once
#include <defs.h>
#include <vector>
#include <stack>
#include <string>

namespace rs {
	class instruction_array {
		public:
			instruction_array(const context_parameters& params);
			~instruction_array();

			struct instruction {
				instruction();
				instruction(rs_instruction i);

				instruction& arg(rs_register reg);
				instruction& arg(variable_id var);
				
				rs_instruction code;

				union {
					rs_register reg;
					variable_id var;
				} args[4];
				bool arg_is_register[4];
				u8 arg_count;
			};

			void backup();
			void restore();
			void commit();

			instruction& append(const instruction& i, const std::string& file, u32 line, u32 col, const std::string& lineText);
			inline instruction  operator[] (size_t idx) const { return m_arr[idx]; }
			inline instruction& operator[] (size_t idx)       { return m_arr[idx]; }
			inline size_t count() const { return m_count; }

			struct instruction_src {
				std::string file;
				u32 line;
				u32 col;
				std::string lineText;
			};

			instruction_src instruction_source(u32 idx) const;


		protected:
			struct internal_instruction_src {
				u32 fileIdx;
				u32 lineIdx;
				u32 line;
				u32 col;
			};
			instruction* m_arr;
			internal_instruction_src* m_srcMap;
			size_t m_capacity;
			size_t m_count;
			size_t m_blockSize;

			std::vector<std::string> m_srcFiles;
			std::vector<std::string> m_srcLines;
			std::stack<size_t> m_restorePoints;
	};
};