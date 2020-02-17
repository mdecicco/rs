#include <instruction_sets.h>
#include <context.h>
#include <execution_state.h>
#include <script_object.h>
#include <script_function.h>
using namespace std;

#define max(a, b) ((a) < (b) ? (b) : (a))
#define min(a, b) ((a) > (b) ? (b) : (a))

namespace rs {
	using mem_var = context_memory::mem_var;
	using instruction = instruction_array::instruction;

	inline void vstore(context* ctx, variable_id dst, variable_id src) {
		mem_var src_v = ctx->memory->get(src);
		ctx->memory->set(dst, src_v.type, src_v.size, src_v.data);
	}

	inline integer_type ipow(integer_type base, integer_type exp) {
		integer_type result = 1;
		if (exp < 0) {
			// yike
			return powf((decimal_type)base, (decimal_type)exp);
		}

		for (;;) {
			if (exp & 1) result *= base;
			exp >>= 1;
			if (!exp) break;
			base *= base;
		}

		return result;
	}

	inline void nan_result(context* ctx, register_type* registers) {
		decimal_type nan = rs_nan;
		variable_id nan_id = ctx->memory->set(rs_builtin_type::t_decimal, sizeof(decimal_type), &nan);
		registers[rs_register::rvalue] = nan_id;
	}

	inline void df_store(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var src;
		if (i->arg_is_register[1]) src = ctx->memory->get(registers[i->args[1].reg]);
		else src = ctx->memory->get(i->args[1].var);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		ctx->memory->set(dst_id, src.type, src.size, src.data);
	}
	inline void df_addProto(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		throw runtime_exception(
			format("'%s' is not an object", var_tostring(a).c_str()),
			state,
			*i
		);
	}
	inline void df_newObj(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		auto obj = new script_object(ctx);
		obj->set_id(ctx->memory->set(rs_builtin_type::t_object, sizeof(script_object*), obj));
		state->registers()[rs_register::lvalue] = obj->id();
	}
	inline void df_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		throw runtime_exception(
			format("'%s' is not an object", var_tostring(a).c_str()),
			state,
			*i
		);
	}
	inline void df_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a;
		if (i->arg_is_register[0]) a = ctx->memory->get(registers[i->args[0].reg]);
		else a = ctx->memory->get(i->args[0].var);

		throw runtime_exception(
			format("'%s' is not an object", var_tostring(a).c_str()),
			state,
			*i
		);
	}
	inline void df_move(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		if (i->arg_is_register[1]) registers[i->args[0].reg] = registers[i->args[1].reg];
		else registers[i->args[0].reg] = i->args[1].var;
	}
	inline void df_or(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a;
		if (i->arg_is_register[1]) a = ctx->memory->get(registers[i->args[1].reg]);
		else a = ctx->memory->get(i->args[1].var);

		mem_var b;
		if (i->arg_is_register[0]) b = ctx->memory->get(registers[i->args[0].reg]);
		else b = ctx->memory->get(i->args[0].var);

		size_t x = 0;
		if (a.size) {
			for (;x < a.size;x++) {
				if (((u8*)a.data)[x]) {
					u8 v = 1;
					registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
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
					registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
					// todo: add allocated rvalue to scope allocated variable ids 
					return;
				}
			}
		}

		u8 v = 0;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void df_and(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a;
		if (i->arg_is_register[1]) a = ctx->memory->get(registers[i->args[1].reg]);
		else a = ctx->memory->get(i->args[1].var);

		mem_var b;
		if (i->arg_is_register[0]) b = ctx->memory->get(registers[i->args[0].reg]);
		else b = ctx->memory->get(i->args[0].var);

		size_t x = 0;
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
					registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
					// todo: add allocated rvalue to scope allocated variable ids 
					return;
				}
			}
		}

		u8 v = 0;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void df_compare(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a;
		if (i->arg_is_register[1]) a = ctx->memory->get(registers[i->args[1].reg]);
		else a = ctx->memory->get(i->args[1].var);

		mem_var b;
		if (i->arg_is_register[0]) b = ctx->memory->get(registers[i->args[0].reg]);
		else b = ctx->memory->get(i->args[0].var);

		if (a.size && b.size && a.size == b.size) {
			for (size_t x = 0;x < a.size;x++) {
				if (((u8*)a.data)[x] != ((u8*)b.data)[x]) {
					u8 v = 0;
					registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
					// todo: add allocated rvalue to scope allocated variable ids 
					return;
				}
			}

			u8 v = 1;
			registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
			// todo: add allocated rvalue to scope allocated variable ids 
			return;
		}

		u8 v = 0;
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_bool, 1, &v);
		// todo: add allocated rvalue to scope allocated variable ids 
	}

	inline void df_orEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		df_or(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);

	}
	inline void df_andEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		df_and(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);
	}
	inline void df_branch(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var cond;
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
	inline void df_call(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();
		mem_var func_var;
		if (i->arg_is_register[0]) func_var = ctx->memory->get(registers[i->args[0].reg]);
		else func_var = ctx->memory->get(i->args[0].var);

		script_function* func = (script_function*)func_var.data;
		if (func->cpp_callback) {
			script_object* this_obj = nullptr;
			variable_id this_obj_id = registers[rs_register::this_obj];
			if (this_obj_id != 0) {
				mem_var& obj = ctx->memory->get(this_obj_id);
				if (obj.type == rs_builtin_type::t_object) this_obj = (script_object*)obj.data;
			}

			func_args args;
			args.state = state;
			args.context = ctx;
			args.self = this_obj;
			for (u8 arg = 0;arg < 8;arg++) {
				variable_id arg_id = registers[rs_register::parameter0 + arg];
				args.parameters.push({
					ctx->memory->get(arg_id),
					arg_id
				});
			}

			registers[rs_register::return_value] = func->cpp_callback(&args);
		} else {
			vstore(ctx, registers[rs_register::return_address], registers[rs_register::instruction_address]);
		
			state->push_state();
			state->push_scope();
		
			vstore(ctx, registers[rs_register::instruction_address], func->entry_point_id);
		}
	}
	inline void df_jump(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();
		vstore(ctx, registers[rs_register::instruction_address], i->args[0].var);
	}
	inline void df_ret(execution_state* state, instruction* i) {
		state->pop_scope();
		state->pop_state(rs_register::null_register);
		register_type* registers = state->registers();
		vstore(state->ctx(), registers[rs_register::instruction_address], registers[rs_register::return_address]);
	}
	inline void df_pushState(execution_state* state, instruction* i) {
		state->push_state();
	}
	inline void df_popState(execution_state* state, instruction* i) {
		state->pop_state(i->args[0].reg);
	}
	inline void df_pushScope(execution_state* state, instruction* i) {
		state->push_scope();
	}
	inline void df_popScope(execution_state* state, instruction* i) {
		state->pop_scope();
	}

	inline void num_add(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);
		
		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
		) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			integer_type result = (*(integer_type*)a.data) + (*(integer_type*)b.data);
			result_id = ctx->memory->set(type, sizeof(integer_type), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			av += bv;
			result_id = ctx->memory->set(type, sizeof(decimal_type), &av);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_sub(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
		) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			integer_type result = (*(integer_type*)a.data) - (*(integer_type*)b.data);
			result_id = ctx->memory->set(type, sizeof(integer_type), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			av -= bv;
			result_id = ctx->memory->set(type, sizeof(decimal_type), &av);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_mul(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			integer_type result = (*(integer_type*)a.data) * (*(integer_type*)b.data);
			result_id = ctx->memory->set(type, sizeof(integer_type), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			av *= bv;
			result_id = ctx->memory->set(type, sizeof(decimal_type), &av);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_div(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			integer_type result = (*(integer_type*)a.data) / (*(integer_type*)b.data);
			result_id = ctx->memory->set(type, sizeof(integer_type), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			av /= bv;
			result_id = ctx->memory->set(type, sizeof(decimal_type), &av);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_mod(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			integer_type result = (*(integer_type*)a.data) % (*(integer_type*)b.data);
			result_id = ctx->memory->set(type, sizeof(integer_type), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			av = fmod(av, bv);
			result_id = ctx->memory->set(type, sizeof(decimal_type), &av);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_pow(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			integer_type result = ipow((*(integer_type*)a.data), (*(integer_type*)b.data));
			result_id = ctx->memory->set(type, sizeof(integer_type), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			av = ::pow(av, bv);
			result_id = ctx->memory->set(type, sizeof(decimal_type), &av);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_less(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			u8 result = (*(integer_type*)a.data) < (*(integer_type*)b.data) ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			u8 result = av < bv ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_greater(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			u8 result = (*(integer_type*)a.data) > (*(integer_type*)b.data) ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			u8 result = av > bv ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_compare(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			u8 result = (*(integer_type*)a.data) == (*(integer_type*)b.data) ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			u8 result = av == bv ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}

	inline void num_addEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		num_add(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);
	}
	inline void num_subEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		num_sub(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);
	}
	inline void num_mulEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		num_mul(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);
	}
	inline void num_divEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		num_div(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);
	}
	inline void num_modEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		num_mod(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);
	}
	inline void num_powEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		num_pow(state, i);

		variable_id dst_id = 0;
		if (i->arg_is_register[0]) dst_id = registers[i->args[0].reg];
		else dst_id = i->args[0].var;

		vstore(ctx, dst_id, registers[rs_register::rvalue]);
	}
	inline void num_lessEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			u8 result = (*(integer_type*)a.data) <= (*(integer_type*)b.data) ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			u8 result = av <= bv ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_greaterEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		variable_id result_id = 0;
		if (type == rs_builtin_type::t_integer) {
			u8 result = (*(integer_type*)a.data) >= (*(integer_type*)b.data) ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type != type) {
				av = *(integer_type*)a.data;
				bv = *(decimal_type*)b.data;
			} else if (b.type != type) {
				av = *(decimal_type*)a.data;
				bv = *(integer_type*)b.data;
			} else {
				av = *(decimal_type*)a.data;
				bv = *(decimal_type*)b.data;
			}

			u8 result = av >= bv ? 1 : 0;
			result_id = ctx->memory->set(rs_builtin_type::t_bool, sizeof(u8), &result);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_inc(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		if (!a.type || a.size == 0 || a.type > rs_builtin_type::t_decimal) return nan_result(ctx, registers);

		variable_id result_id = 0;
		if (a.type == rs_builtin_type::t_integer) {
			integer_type result = *(integer_type*)a.data + integer_type(1);
			result_id = ctx->memory->set(rs_builtin_type::t_integer, sizeof(integer_type), &result);
		} else {
			decimal_type result = *(decimal_type*)a.data + decimal_type(1);
			result_id = ctx->memory->set(rs_builtin_type::t_decimal, sizeof(decimal_type), &result);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}
	inline void num_dec(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		if (!a.type || a.size == 0 || a.type > rs_builtin_type::t_decimal) return nan_result(ctx, registers);

		variable_id result_id = 0;
		if (a.type == rs_builtin_type::t_integer) {
			integer_type result = *(integer_type*)a.data - integer_type(1);
			result_id = ctx->memory->set(rs_builtin_type::t_integer, sizeof(integer_type), &result);
		} else {
			decimal_type result = *(decimal_type*)a.data - decimal_type(1);
			result_id = ctx->memory->set(rs_builtin_type::t_decimal, sizeof(decimal_type), &result);
		}

		if (!result_id) nan_result(ctx, registers);
		else {
			registers[rs_register::rvalue] = result_id;
		}
		// todo: add allocated rvalue to scope allocated variable ids 
	}

	inline void obj_addProto(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);

		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var b = ctx->memory->get(b_id);

		script_object* obj = (script_object*)a.data;
		object_prototype* proto = (object_prototype*)b.data;

		obj->add_prototype(proto, false, nullptr, 0);
	}
	inline void obj_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var a = ctx->memory->get(a_id);
		mem_var b = ctx->memory->get(b_id);

		string propName;
		switch (b.type) {
			case rs_builtin_type::t_string: {
				propName = string((char*)b.data, b.size);
				break;
			}
			case rs_builtin_type::t_integer: {
				propName = format("%d", *(integer_type*)b.data);
				break;
			}
			case rs_builtin_type::t_decimal: {
				propName = format("%f", *(decimal_type*)b.data);
				break;
			}
			default: {
				throw runtime_exception(
					format("'%s' is not a valid property name or index", var_tostring(b).c_str()),
					state,
					*i
				);
			}
		}

		script_object* obj = (script_object*)a.data;
		variable_id prop_id = obj->property(propName);
		if (prop_id == 0) {
			prop_id = obj->proto_property(propName);
			if (prop_id == 0) {
				throw runtime_exception(
					format("Object has no property named '%s'", propName.c_str()),
					state,
					*i
				);
			}
		}

		registers[rs_register::lvalue] = prop_id;
	}
	inline void obj_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var a = ctx->memory->get(a_id);
		mem_var b = ctx->memory->get(b_id);

		string propName;
		switch (b.type) {
			case rs_builtin_type::t_string: {
				propName = string((char*)b.data, b.size);
				break;
			}
			case rs_builtin_type::t_integer: {
				propName = format("%d", *(integer_type*)b.data);
				break;
			}
			case rs_builtin_type::t_decimal: {
				propName = format("%f", *(decimal_type*)b.data);
				break;
			}
			default: {
				throw runtime_exception(
					format("'%s' is not a valid property name or index", var_tostring(b).c_str()),
					state,
					*i
				);
			}
		}

		script_object* obj = (script_object*)a.data;
		variable_id prop_id = obj->property(propName);
		if (prop_id == 0) {
			prop_id = obj->define_property(propName, rs_builtin_type::t_null, 0, nullptr);
		}

		registers[rs_register::lvalue] = prop_id;
	}

	inline void str_add(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var av = ctx->memory->get(a_id);
		string a((char*)av.data, av.size);
		string b = var_tostring(ctx->memory->get(b_id));
		a += b;

		char* result = new char[a.length()];
		memcpy(result, a.c_str(), a.length());
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_string, a.length(), result);
	}
	inline void str_addEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var& av = ctx->memory->get(a_id);
		string a((char*)av.data, av.size);
		string b = var_tostring(ctx->memory->get(b_id));
		a += b;

		char* result = new char[a.length()];
		memcpy(result, a.c_str(), a.length());
		registers[rs_register::rvalue] = ctx->memory->set(rs_builtin_type::t_string, a.length(), result);

		delete [] (char*)av.data;
		av.data = new char[a.length()];
		memcpy(av.data, a.c_str(), a.length());
	}
	inline void str_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg] : i->args[0].var;
		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg] : i->args[1].var;
		mem_var a = ctx->memory->get(a_id);
		mem_var b = ctx->memory->get(b_id);

		integer_type idx = -1;
		if (b.type == rs_builtin_type::t_integer) {
			idx = *(integer_type*)b.data;
		} else {
			throw runtime_exception(
				format("'%s' is not a valid string index", var_tostring(b).c_str()),
				state,
				*i
			);
			return;
		}

		if (idx < 0 || idx >= a.size) {
			throw runtime_exception(
				format("String index out of range"),
				state,
				*i
			);
			return;
		}

		char* result = new char[1];
		*result = ((char*)a.data)[idx];
		registers[rs_register::lvalue] = ctx->memory->set(rs_builtin_type::t_string, 1, result);
	}



	void add_default_instruction_set(context* ctx) {
		ctx->define_instruction(0, rs_instruction::store, df_store);
		ctx->define_instruction(0, rs_instruction::newObj, df_newObj);
		ctx->define_instruction(0, rs_instruction::prop, df_prop);
		ctx->define_instruction(0, rs_instruction::propAssign, df_propAssign);
		ctx->define_instruction(0, rs_instruction::move, df_move);
		ctx->define_instruction(0, rs_instruction::or, df_or);
		ctx->define_instruction(0, rs_instruction::and, df_and);
		ctx->define_instruction(0, rs_instruction::compare, df_compare);
		ctx->define_instruction(0, rs_instruction::orEq, df_orEq);
		ctx->define_instruction(0, rs_instruction::andEq, df_andEq);
		ctx->define_instruction(0, rs_instruction::branch, df_branch);
		ctx->define_instruction(0, rs_instruction::call, df_call);
		ctx->define_instruction(0, rs_instruction::jump, df_jump);
		ctx->define_instruction(0, rs_instruction::ret, df_ret);
		ctx->define_instruction(0, rs_instruction::pushState, df_pushState);
		ctx->define_instruction(0, rs_instruction::popState, df_popState);
		ctx->define_instruction(0, rs_instruction::pushScope, df_pushScope);
		ctx->define_instruction(0, rs_instruction::popScope, df_popScope);
	}

	void add_number_instruction_set(context* ctx) {
		rs_builtin_type number_types[2] = {
			rs_builtin_type::t_integer,
			rs_builtin_type::t_decimal
		};

		for (u8 i = 0;i < 2;i++) {
			rs_builtin_type type = number_types[i];
			ctx->define_instruction(type, rs_instruction::add, num_add);
			ctx->define_instruction(type, rs_instruction::sub, num_sub);
			ctx->define_instruction(type, rs_instruction::mul, num_mul);
			ctx->define_instruction(type, rs_instruction::div, num_div);
			ctx->define_instruction(type, rs_instruction::mod, num_mod);
			ctx->define_instruction(type, rs_instruction::pow, num_pow);
			ctx->define_instruction(type, rs_instruction::less, num_less);
			ctx->define_instruction(type, rs_instruction::greater, num_greater);
			ctx->define_instruction(type, rs_instruction::compare, num_compare);

			ctx->define_instruction(type, rs_instruction::addEq, num_addEq);
			ctx->define_instruction(type, rs_instruction::subEq, num_subEq);
			ctx->define_instruction(type, rs_instruction::mulEq, num_mulEq);
			ctx->define_instruction(type, rs_instruction::divEq, num_divEq);
			ctx->define_instruction(type, rs_instruction::modEq, num_modEq);
			ctx->define_instruction(type, rs_instruction::powEq, num_powEq);
			ctx->define_instruction(type, rs_instruction::lessEq, num_lessEq);
			ctx->define_instruction(type, rs_instruction::greaterEq, num_greaterEq);

			ctx->define_instruction(type, rs_instruction::inc, num_inc);
			ctx->define_instruction(type, rs_instruction::dec, num_dec);
		}
	}

	void add_object_instruction_set(context* ctx) {
		ctx->define_instruction(rs_builtin_type::t_object, rs_instruction::addProto, obj_addProto);
		ctx->define_instruction(rs_builtin_type::t_object, rs_instruction::prop, obj_prop);
		ctx->define_instruction(rs_builtin_type::t_object, rs_instruction::propAssign, obj_propAssign);
	}

	void add_string_instruction_set(context* ctx) {
		ctx->define_instruction(rs_builtin_type::t_string, rs_instruction::prop, str_prop);
		ctx->define_instruction(rs_builtin_type::t_string, rs_instruction::add, str_add);
		ctx->define_instruction(rs_builtin_type::t_string, rs_instruction::addEq, str_addEq);
	}
};