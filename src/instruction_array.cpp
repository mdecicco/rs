#include <instruction_array.h>
#include <parse_utils.h>
using namespace std;

namespace rs {
	instruction_array::instruction_array(const context_parameters& params) {
		m_capacity = params.instruction_array.initial_size;
		m_blockSize = params.instruction_array.resize_amount;
		m_count = 0;

		m_arr = new instruction[m_capacity];
		m_srcMap = new internal_instruction_src[m_capacity];
	}

	instruction_array::~instruction_array() {
		delete [] m_arr;
		delete [] m_srcMap;
		m_arr = nullptr;
		m_srcMap = nullptr;
		m_count = 0;
		m_capacity = 0;
		m_blockSize = 0;
	}



	instruction_array::instruction::instruction() {
		code = rs_instruction::null_instruction;
		memset(args, 0, sizeof(args));
		memset(arg_is_register, 0, sizeof(bool) * 4);
		arg_count = 0;
	}

	instruction_array::instruction::instruction(rs_instruction i) {
		code = i;
		memset(args, 0, sizeof(args));
		memset(arg_is_register, 0, sizeof(bool) * 4);
		arg_count = 0;
	}



	instruction_array::instruction& instruction_array::instruction::arg(rs_register reg) {
		if (arg_count == 4) return *this;
		args[arg_count].reg = reg;
		arg_is_register[arg_count++] = true;
		return *this;
	}

	instruction_array::instruction& instruction_array::instruction::arg(variable_id var) {
		if (arg_count == 4) return *this;
		args[arg_count].var = var;
		arg_is_register[arg_count++] = false;
		return *this;
	}



	void instruction_array::backup() {
		m_restorePoints.push(m_count);
	}

	void instruction_array::restore() {
		m_count = m_restorePoints.top();
		m_restorePoints.pop();
	}

	void instruction_array::commit() {
		m_restorePoints.pop();
	}

	instruction_array::instruction& instruction_array::append(const instruction& i, const string& file, u32 line, u32 col, const string& lineText) {
		if (m_count + 1 > rs_integer_max) {
			throw parse_exception(
				format("Script compiler was configured to use integers that are too small to contain more than %llu instructions. Update the configuration and recompile to increase the instruction capacity.", rs_integer_max),
				file,
				lineText,
				line,
				col
			);
		}

		if (m_count + 1 == m_capacity) {
			instruction* newArr = new instruction[m_capacity + m_blockSize];
			memcpy(newArr, m_arr, sizeof(instruction) * m_count);
			delete [] m_arr;
			m_arr = newArr;

			internal_instruction_src* newSrcMap = new internal_instruction_src[m_capacity + m_blockSize];
			memcpy(newSrcMap, m_srcMap, sizeof(internal_instruction_src) * m_count);
			delete m_srcMap;
			m_srcMap = newSrcMap;
		}

		size_t idx = m_count++;
		m_arr[idx] = i;
		m_arr[idx].idx = idx;
		internal_instruction_src src = {
			0,
			0,
			line,
			col
		};

		u32 vidx = 0;
		for (const string& existing : m_srcFiles) {
			if (file == existing) {
				src.fileIdx = vidx;
				vidx = 0;
				break;
			}
			vidx++;
		}
		if (vidx == m_srcFiles.size()) {
			src.fileIdx = m_srcFiles.size();
			m_srcFiles.push_back(file);
		}

		vidx = 0;
		for (const string& existing : m_srcLines) {
			if (lineText == existing) {
				src.lineIdx = vidx;
				break;
			}
			vidx++;
		}
		if (vidx == m_srcLines.size()) {
			src.lineIdx = m_srcLines.size();
			m_srcLines.push_back(lineText);
		}

		m_srcMap[idx] = src;
		return m_arr[idx];
	}

	instruction_array::instruction_src instruction_array::instruction_source(integer_type idx) const {
		instruction_src src;
		src.col = m_srcMap[idx].col;
		src.line = m_srcMap[idx].line;
		src.file = m_srcFiles[m_srcMap[idx].fileIdx];
		src.lineText = m_srcLines[m_srcMap[idx].lineIdx];
		return src;
	}
};