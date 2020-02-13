#include <execution_state.h>
#include <context.h>

namespace rs {

	std::string v2s(rs::context_memory::mem_var& v) {
		std::string val;
		if (!v.data) val = "undefined";
		else {
			switch (v.type) {
			case rs::rs_builtin_type::t_string: {
				val = std::string((char*)v.data, v.size);
				break;
			}
			case rs::rs_builtin_type::t_int8: {
				val = rs::format("%d", *(char*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint8: {
				val = rs::format("%u", *(unsigned char*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_int16: {
				val = rs::format("%d", *(short*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint16: {
				val = rs::format("%u", *(unsigned short*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_int32: {
				val = rs::format("%d", *(rs::i32*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint32: {
				val = rs::format("%u", *(rs::u32*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_int64: {
				val = rs::format("%ll", *(rs::i64*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint64: {
				val = rs::format("%llu", *(rs::u64*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_float: {
				val = rs::format("%f", *(float*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_double: {
				val = rs::format("%f", *(double*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_class: {
				val = "class";
				break;
			}
			case rs::rs_builtin_type::t_function: {
				val = "function";
				break;
			}
			case rs::rs_builtin_type::t_object: {
				val = "object";
				break;
			}
			default: {
				val = "scripted type";
				break;
			}
			}
		}

		return val;
	}

	execution_state::execution_state(const context_parameters& params, context* ctx) {
		m_ctx = ctx;
		m_stack_idx = 0;
		m_stack_depth = params.execution.max_stack_depth;

		const size_t rc = rs_register::register_count;
		m_stack = (register_type(*)[rc])new register_type[rc * m_stack_depth];
		memset(m_stack, 0, m_stack_depth * rc * sizeof(rs_register));
	}

	execution_state::~execution_state() {
		delete [] m_stack;
	}

	void execution_state::execute_all(u64 entry_point) {
		u64 instruction_addr = entry_point;
		variable_id instruction_addr_id = m_ctx->memory->set(rs_builtin_type::t_uint64, sizeof(u64), &instruction_addr);
		registers()[rs_register::instruction_address] = instruction_addr_id;
		u64 return_addr = UINT64_MAX;
		variable_id return_addr_id = m_ctx->memory->set(rs_builtin_type::t_uint64, sizeof(u64), &return_addr);
		registers()[rs_register::return_address] = return_addr_id;

		const context::instruction_set* default_iset = m_ctx->get_instruction_set(0);

		m_last_printed_col = -1;
		m_last_printed_line = -1;
		u64* iaddr = (u64*)m_ctx->memory->get(instruction_addr_id).data;
		while ((*iaddr) < m_ctx->instructions->count()) {
			auto& instruction = (*m_ctx->instructions)[(*iaddr)++];

			print_instruction((*iaddr) - 1, instruction);

			const context::instruction_set* iset = default_iset;
			if (instruction.arg_count > 0) {
				context_memory::mem_var ba;
				if (instruction.arg_is_register[0]) ba = m_ctx->memory->get(registers()[instruction.args[0].reg]);
				else ba = m_ctx->memory->get(instruction.args[0].var);
				iset = m_ctx->get_instruction_set(ba.type);
			}
			

			instruction_callback icb = iset->callbacks[instruction.code];
			if (!icb) icb = default_iset->callbacks[instruction.code];

			if (icb) icb(this, &instruction);
			else {
				printf("woah there\n");
				return;
			}
		}
	}

	void execution_state::execute_next() {
		
	}

	void execution_state::push_state() {
		if (m_stack_idx == m_stack_depth - 1) {
			printf("uh oh\n");
			return;
		}

		register_type* prev = m_stack[m_stack_idx++];
		register_type* now = m_stack[m_stack_idx];
		for (u8 x = 0;x < 8;x++) now[rs_register::parameter0 + x] = prev[rs_register::parameter0 + x];
		now[rs_register::instruction_address] = prev[rs_register::instruction_address];


		u64 return_addr = UINT64_MAX;
		variable_id return_addr_id = m_ctx->memory->set(rs_builtin_type::t_uint64, sizeof(u64), &return_addr);
		now[rs_register::return_address] = return_addr_id;
	}

	void execution_state::pop_state(rs_register persist) {
		if (m_stack_idx == 0) {
			printf("oh boy\n");
			return;
		}
		register_type* prev = m_stack[m_stack_idx--];
		register_type* now = m_stack[m_stack_idx];
		now[rs_register::return_value] = prev[rs_register::return_value];
		now[rs_register::instruction_address] = prev[rs_register::instruction_address];
		now[persist] = prev[persist];

		m_ctx->memory->deallocate(prev[rs_register::return_address]);
		memset(prev, 0, rs_register::register_count * sizeof(rs_register));

	}

	void execution_state::push_scope() {
	}

	void execution_state::pop_scope() {
	}

	void execution_state::print_instruction(u64 idx, const instruction_array::instruction& i) {
		static const char* istr[] = {
			"null",
			"store",
			"prop",
			"move",
			"add",
			"sub",
			"mul",
			"div",
			"mod",
			"pow",
			"or",
			"and",
			"less",
			"greater",
			"addEq",
			"subEq",
			"mulEq",
			"divEq",
			"modEq",
			"powEq",
			"orEq",
			"andEq",
			"lessEq",
			"greaterEq",
			"inc",
			"dec",
			"branch",
			"call",
			"ret",
			"pushState",
			"popState",
			"pushScope",
			"popScope"
		};
		static const char* rstr[] = {
			"null",
			"this_obj",
			"return_val",
			"return_address",
			"instruction_address",
			"lvalue",
			"rvalue",
			"parameter0",
			"parameter1",
			"parameter2",
			"parameter3",
			"parameter4",
			"parameter5",
			"parameter6",
			"parameter7",
			"parameter_count"
		};

		auto src = m_ctx->instructions->instruction_source(idx);

		if (src.line != m_last_printed_line || src.col != m_last_printed_col) {
			printf("\n\n%s\n", src.lineText.c_str());
		}
		m_last_printed_line = src.line;
		m_last_printed_col = src.col;
		for (int c = 0;c < int(src.col);c++) printf(" ");
		printf("^ ");
		printf(istr[i.code]);
		for (int a = 0;a < i.arg_count;a++) {
			if (i.arg_is_register[a]) {
				variable_id vid = registers()[i.args[a].reg];
				auto& v = m_ctx->memory->get(vid);
				printf(" $%s (#%llu) (%s)", rstr[i.args[a].reg], vid, v2s(v).c_str());
			}
			else {
				auto& v = m_ctx->memory->get(i.args[a].var);
				printf(" #%llu (%s)", i.args[a].var, v2s(v).c_str());
			}
		}
		printf("\n");
	}
};