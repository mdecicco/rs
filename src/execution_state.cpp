#include <execution_state.h>
#include <context.h>
#include <script_object.h>
using namespace std;

namespace rs {
	runtime_exception::runtime_exception(const string& error, execution_state* state, const instruction_array::instruction& i) {
		text = error;
		auto src = state->ctx()->instructions->instruction_source(i.idx);
		file = src.file;
		line = src.line;
		col = src.col;
		lineText = src.lineText;
		has_source_info = true;
	}

	runtime_exception::runtime_exception(const string& error, execution_state* state) {
		text = error;

		auto src = state->ctx()->instructions->instruction_source(state->instruction_addr());
		file = src.file;
		line = src.line;
		col = src.col;
		lineText = src.lineText;
		has_source_info = true;
	}

	runtime_exception::runtime_exception(const string& error) {
		text = error;
		file = "unknown";
		line = 0;
		col = 0;
		lineText = "";
		has_source_info = false;
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

	void execution_state::execute(integer_type entry_point, integer_type exit_point) {
		integer_type instruction_addr = entry_point;
		variable_id instruction_addr_id = m_ctx->memory->set(rs_builtin_type::t_integer, sizeof(integer_type), &instruction_addr);
		registers()[rs_register::instruction_address] = instruction_addr_id;
		integer_type return_addr = rs_integer_max;
		variable_id return_addr_id = m_ctx->memory->set(rs_builtin_type::t_integer, sizeof(integer_type), &return_addr);
		registers()[rs_register::return_address] = return_addr_id;

		const context::instruction_set* default_iset = m_ctx->get_instruction_set(0);

		m_last_printed_col = -1;
		m_last_printed_line = -1;
		integer_type& iaddr = *(integer_type*)m_ctx->memory->get(instruction_addr_id).data;
		instruction_array& iarr = *m_ctx->instructions;
		if (exit_point == rs_integer_max) exit_point = iarr.count();
		while (iaddr < exit_point) {
			register_type* registers = this->registers();
			m_current_instruction_idx = iaddr++;

			auto& instruction = iarr[m_current_instruction_idx];
			print_instruction(m_current_instruction_idx, instruction);
			if (instruction.code == rs_instruction::null_instruction) continue;


			const context::instruction_set* iset = default_iset;
			if (instruction.arg_count > 0) {
				if (instruction.arg_is_register[0]) {
					register_type& reg = registers[instruction.args[0].reg];
					if (reg.type == rs_builtin_type::t_null) {
						mem_var ba = m_ctx->memory->get(reg.data.v);
						iset = m_ctx->get_instruction_set(ba.type);
					} else iset = m_ctx->get_instruction_set(reg.type);
				} else {
					mem_var ba = m_ctx->memory->get(instruction.args[0].var);
					iset = m_ctx->get_instruction_set(ba.type);
				}
			}
			

			instruction_callback icb = iset->callbacks[instruction.code];
			if (!icb) icb = default_iset->callbacks[instruction.code];

			if (icb) icb(this, &instruction);
			else {
				throw runtime_exception(
					"No instruction exists for this operation",
					this,
					instruction
				);
			}
		}
	}

	void execution_state::push_state() {
		if (m_stack_idx == m_stack_depth - 1) {
			throw runtime_exception(
				"Maximum stack depth reached",
				this
			);
		}

		register_type* prev = m_stack[m_stack_idx++];
		register_type* now = m_stack[m_stack_idx];
		now[rs_register::lvalue] = prev[rs_register::lvalue];
		now[rs_register::rvalue] = prev[rs_register::rvalue];
		now[rs_register::this_obj] = prev[rs_register::this_obj];
		for (u8 x = 0;x < 8;x++) now[rs_register::parameter0 + x] = prev[rs_register::parameter0 + x];
		now[rs_register::instruction_address] = prev[rs_register::instruction_address];


		integer_type return_addr = rs_integer_max;
		variable_id return_addr_id = m_ctx->memory->set(rs_builtin_type::t_integer, sizeof(integer_type), &return_addr);
		now[rs_register::return_address] = return_addr_id;
	}

	void execution_state::pop_state(rs_register persist) {
		if (m_stack_idx == 0) {
			throw runtime_exception(
				"Minimum stack depth reached",
				this
			);
		}
		register_type* prev = m_stack[m_stack_idx--];
		register_type* now = m_stack[m_stack_idx];
		now[rs_register::return_value] = prev[rs_register::return_value];
		now[rs_register::instruction_address] = prev[rs_register::instruction_address];
		now[persist] = prev[persist];

		m_ctx->memory->deallocate(prev[rs_register::return_address].data.v);
		memset(prev, 0, rs_register::register_count * sizeof(rs_register));

	}

	void execution_state::push_scope() {
	}

	void execution_state::pop_scope() {
	}

	void execution_state::print_instruction(integer_type idx, const instruction_array::instruction& i) {
		static const char* istr[] = {
			"null",
			"store",
			"newObj",
			"addProto",
			"prop",
			"propAssign",
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
			"compare",
			"inc",
			"dec",
			"branch",
			"clearParams",
			"call",
			"jump",
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
				register_type& reg = registers()[i.args[a].reg];
				if (reg.type == rs_builtin_type::t_null) {
					auto& v = m_ctx->memory->get(reg.data.v);
					printf(" $%s (#%llu) (%s)", rstr[i.args[a].reg], reg.data.v, var_tostring(v).c_str());
				} else {
					switch (reg.type) {
						case rs_builtin_type::t_integer: {
							printf(" $%s (%d)", rstr[i.args[a].reg], reg.data.i);
							break;
						}
						case rs_builtin_type::t_decimal: {
							printf(" $%s (%f)", rstr[i.args[a].reg], reg.data.d);
							break;
						}
						case rs_builtin_type::t_bool: {
							printf(" $%s (%s)", rstr[i.args[a].reg], reg.data.b ? "true" : "false");
							break;
						}
						default: {
							printf(" $%s (<invalid value>)", rstr[i.args[a].reg]);
							break;
						}
					};
				}
			}
			else {
				auto& v = m_ctx->memory->get(i.args[a].var);
				printf(" #%llu (%s)", i.args[a].var, var_tostring(v).c_str());
			}
		}
		printf(" [stack: %d]\n", m_stack_idx);
	}
};