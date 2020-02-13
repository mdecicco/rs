#include <instruction_sets.h>
#include <context.h>
#include <execution_state.h>

namespace rs {
	void vstore(context* ctx, variable_id dst, variable_id src) {
		context_memory::mem_var src_v = ctx->memory->get(src);
		ctx->memory->set(dst, src_v.type, src_v.size, src_v.data);
	}

	void df_store(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var src;
		if (i->arg_is_register[1]) src = ctx->memory->get(registers[i->args[1].reg]);
		else src = ctx->memory->get(i->args[1].var);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		ctx->memory->set(dst_id, src.type, src.size, src.data);
	}
	void df_prop(execution_state* state, instruction_array::instruction* i) {
	}
	void df_move(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		if (i->arg_is_register[1]) registers[i->args[0].reg] = registers[i->args[1].reg];
		else registers[i->args[0].reg] = i->args[1].var;
	}
	void df_or(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[1]) a = ctx->memory->get(registers[i->args[1].reg]);
		else a = ctx->memory->get(i->args[1].var);

		context_memory::mem_var b;
		if (i->arg_is_register[0]) b = ctx->memory->get(registers[i->args[0].reg]);
		else b = ctx->memory->get(i->args[0].var);

		size_t x = 0;
		if (a.size) {
			for (;x < a.size;x++) {
				if (((u8*)a.data)[x]) {
					u8 v = 1;
					registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_uint8, 1, &v);
					// todo: add allocated rvalue to scope allocated variable ids 
					return;
				}
			}
		}

		if (b.size) {
			x = 0;
			for (;x < b.size;x++) {
				if (((u8*)b.data)[x]) {
					u8 v = 1;
					registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_uint8, 1, &v);
					// todo: add allocated rvalue to scope allocated variable ids 
					return;
				}
			}
		}

		u8 v = 0;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_uint8, 1, &v);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void df_and(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[1]) a = ctx->memory->get(registers[i->args[1].reg]);
		else a = ctx->memory->get(i->args[1].var);

		context_memory::mem_var b;
		if (i->arg_is_register[0]) b = ctx->memory->get(registers[i->args[0].reg]);
		else b = ctx->memory->get(i->args[0].var);

		size_t x = 0;
		bool is_false = false;
		if (a.size && b.size) {
			for (;x < a.size;x++) {
				if (((u8*)a.data)[x]) {
					break;
				}
			}

			if (x < a.size) {
				x = 0;
				for (;x < b.size;x++) {
					if (((u8*)b.data)[x]) {
						break;
					}
				}
				if (x < b.size) {
					u8 v = 1;
					registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_uint8, 1, &v);
					// todo: add allocated rvalue to scope allocated variable ids 
				}
			}
		}

		u8 v = 0;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_uint8, 1, &v);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void df_orEq(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		df_or(state, i);

		context_memory::mem_var src = ctx->memory->get(registers[rs_register::rvalue]);
		context_memory::mem_var dst;
		if (i->arg_is_register[0]) dst = ctx->memory->get(registers[i->args[0].reg]);
		else dst = ctx->memory->get(i->args[0].var);

		dst.type = src.type;
		if (dst.size != src.size) {
			// todo: resize dest
		}

		memcpy(dst.data, src.data, dst.size);

	}
	void df_andEq(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		df_and(state, i);

		context_memory::mem_var src = ctx->memory->get(registers[rs_register::rvalue]);
		context_memory::mem_var dst;
		if (i->arg_is_register[0]) dst = ctx->memory->get(registers[i->args[0].reg]);
		else dst = ctx->memory->get(i->args[0].var);

		dst.type = src.type;
		if (dst.size != src.size) {
			// todo: resize dest
		}

		memcpy(dst.data, src.data, dst.size);
	}
	void df_branch(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var cond;
		if (i->arg_is_register[0]) cond = ctx->memory->get(registers[i->args[0].reg]);
		else cond = ctx->memory->get(i->args[0].var);

		for (size_t x = 0;x < cond.size;x++) {
			if (((u8*)cond.data)[x]) {
				vstore(ctx, registers[rs_register::instruction_address], i->args[1].var);
				return;
			}
		}
		vstore(ctx, registers[rs_register::instruction_address], i->args[2].var);
	}
	void df_call(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();
		vstore(ctx, registers[rs_register::return_address], registers[rs_register::instruction_address]);
		
		state->push_state();
		state->push_scope();

		vstore(ctx, registers[rs_register::instruction_address], i->args[0].var);
		
	}
	void df_ret(execution_state* state, instruction_array::instruction* i) {
		state->pop_scope();
		state->pop_state(rs_register::null_register);
		register_type* registers = state->registers();
		vstore(state->ctx(), registers[rs_register::instruction_address], registers[rs_register::return_address]);
	}
	void df_pushState(execution_state* state, instruction_array::instruction* i) {
		state->push_state();
	}
	void df_popState(execution_state* state, instruction_array::instruction* i) {
		state->pop_state(i->args[0].reg);
	}
	void df_pushScope(execution_state* state, instruction_array::instruction* i) {
		state->push_scope();
	}
	void df_popScope(execution_state* state, instruction_array::instruction* i) {
		state->pop_scope();
	}

	void i32_add(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		context_memory::mem_var b;
		if (i->arg_is_register[1]) b = ctx->memory->get(registers[i->args[1].reg]);
		else b = ctx->memory->get(i->args[1].var);

		i32 result = (*(i32*)a.data) + (*(i32*)b.data);
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_int32, sizeof(i32), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void i32_sub(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		context_memory::mem_var b;
		if (i->arg_is_register[1]) b = ctx->memory->get(registers[i->args[1].reg]);
		else b = ctx->memory->get(i->args[1].var);

		i32 result = (*(i32*)a.data) - (*(i32*)b.data);
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_int32, sizeof(i32), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void i32_mul(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		context_memory::mem_var b;
		if (i->arg_is_register[1]) b = ctx->memory->get(registers[i->args[1].reg]);
		else b = ctx->memory->get(i->args[1].var);

		i32 result = (*(i32*)a.data) * (*(i32*)b.data);
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_int32, sizeof(i32), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void i32_div(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		context_memory::mem_var b;
		if (i->arg_is_register[1]) b = ctx->memory->get(registers[i->args[1].reg]);
		else b = ctx->memory->get(i->args[1].var);

		i32 result = (*(i32*)a.data) / (*(i32*)b.data);
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_int32, sizeof(i32), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void i32_inc(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		i32 result = (*(i32*)a.data)++;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_int32, sizeof(i32), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void i32_dec(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		i32 result = (*(i32*)a.data)--;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_int32, sizeof(i32), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void i32_less(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		context_memory::mem_var b;
		if (i->arg_is_register[1]) b = ctx->memory->get(registers[i->args[1].reg]);
		else b = ctx->memory->get(i->args[1].var);

		u8 result = (*(i32*)a.data) < (*(i32*)b.data) ? 1 : 0;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_uint8, sizeof(u8), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	void i32_greater(execution_state* state, instruction_array::instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		context_memory::mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		context_memory::mem_var b;
		if (i->arg_is_register[1]) b = ctx->memory->get(registers[i->args[1].reg]);
		else b = ctx->memory->get(i->args[1].var);

		u8 result = (*(i32*)a.data) > (*(i32*)b.data) ? 1 : 0;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_uint8, sizeof(u8), &result);
		// todo: add allocated rvalue to scope allocated variable ids 
	}


	void add_default_instruction_set(context* ctx) {
		ctx->define_instruction(0, rs_instruction::store, df_store);
		ctx->define_instruction(0, rs_instruction::prop, df_prop);
		ctx->define_instruction(0, rs_instruction::move, df_move);
		ctx->define_instruction(0, rs_instruction::or, df_or);
		ctx->define_instruction(0, rs_instruction::and, df_and);
		ctx->define_instruction(0, rs_instruction::orEq, df_orEq);
		ctx->define_instruction(0, rs_instruction::andEq, df_andEq);
		ctx->define_instruction(0, rs_instruction::branch, df_branch);
		ctx->define_instruction(0, rs_instruction::call, df_call);
		ctx->define_instruction(0, rs_instruction::ret, df_ret);
		ctx->define_instruction(0, rs_instruction::pushState, df_pushState);
		ctx->define_instruction(0, rs_instruction::popState, df_popState);
		ctx->define_instruction(0, rs_instruction::pushScope, df_pushScope);
		ctx->define_instruction(0, rs_instruction::popScope, df_popScope);
	}

	void add_i32_instruction_set(context* ctx) {
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::add, i32_add);
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::sub, i32_sub);
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::mul, i32_mul);
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::div, i32_div);
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::inc, i32_inc);
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::dec, i32_dec);
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::less, i32_less);
		ctx->define_instruction(rs_builtin_type::t_int32, rs_instruction::greater, i32_greater);
	}
};