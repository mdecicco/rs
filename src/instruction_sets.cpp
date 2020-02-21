#include <instruction_sets.h>
#include <context.h>
#include <execution_state.h>
#include <script_object.h>
#include <script_function.h>
#include <script_array.h>
#include <prototype.h>
using namespace std;

#define max(a, b) ((a) < (b) ? (b) : (a))
#define min(a, b) ((a) > (b) ? (b) : (a))

namespace rs {
	using instruction = instruction_array::instruction;

	template <typename T>
	T cast_param(context* ctx, register_type* registers, instruction* i, u8 arg_idx) {
		if (i->arg_is_register[arg_idx]) {
			register_type& reg = registers[i->args[arg_idx].reg];
			if (reg.type == rs_builtin_type::t_null) {
				mem_var& v = ctx->memory->get(reg.data.v);
				return (T)v.data;
			} else {
				return (T)&reg.data;
			}
		} else {
			mem_var& v = ctx->memory->get(i->args[arg_idx].var);
			return (T)v.data;
		}

		return T(0);
	}

	mem_var cast_param(context* ctx, register_type* registers, instruction* i, u8 arg_idx) {
		if (i->arg_is_register[arg_idx]) return registers[i->args[arg_idx].reg].ref(ctx);
		return ctx->memory->get(i->args[arg_idx].var);
	}

	bool param_truthiness(context* ctx, register_type* registers, instruction* i, u8 arg_idx) {
		if (i->arg_is_register[arg_idx]) {
			register_type& reg = registers[i->args[arg_idx].reg];
			if (reg.type == rs_builtin_type::t_null) {
				mem_var& v = ctx->memory->get(reg.data.v);
				for (size_t x = 0;x < v.size;x++) {
					if (((u8*)v.data)[x]) return true;
				}
				return false;
			} else {
				switch (reg.type) {
				case rs_builtin_type::t_integer: return reg.data.i != integer_type(0);
				case rs_builtin_type::t_decimal: return reg.data.d != decimal_type(0);
				case rs_builtin_type::t_bool: return reg.data.b;
				default: return false;
				};
			}
		} else {
			mem_var& v = ctx->memory->get(i->args[arg_idx].var);
			for (size_t x = 0;x < v.size;x++) {
				if (((u8*)v.data)[x]) return true;
			}
			return false;
		}

		return false;
	}

	string param_tostring(context* ctx, register_type* registers, instruction* i, u8 arg_idx) {
		if (i->arg_is_register[arg_idx]) {
			register_type& reg = registers[i->args[arg_idx].reg];
			if (reg.type == rs_builtin_type::t_null) return var_tostring(ctx->memory->get(reg.data.v));
			else {
				switch (reg.type) {
					case rs_builtin_type::t_integer: {
						return format("%d", reg.data.i);
						break;
					}
					case rs_builtin_type::t_decimal: {
						return format("%f", reg.data.d);
						break;
					}
					case rs_builtin_type::t_bool: {
						return format("%s", reg.data.b ? "true" : "false");
						break;
					}
					default: {
						return "<invalid value>";
						break;
					}
				};
			}
		} else return var_tostring(ctx->memory->get(i->args[arg_idx].var));

		return "<invalid value>";
	}

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
		registers[rs_register::rvalue] = rs_nan;
	}

	inline void df_store(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id dst = 0;
		if (i->arg_is_register[0]) dst = registers[i->args[0].reg].data.v;
		else dst = i->args[0].var;

		if (i->arg_is_register[1]) {
			register_type& src_reg = registers[i->args[1].reg];
			if (src_reg.type == rs_builtin_type::t_null) vstore(ctx, dst, src_reg.data.v);
			else ctx->memory->set(dst, src_reg.type, type_sizes[src_reg.type], &src_reg.data);
		} else vstore(ctx, dst, i->args[1].var);
	}
	inline void df_newArr(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		if (i->arg_count > 0) {
			if (i->arg_is_register[0]) {
				auto arr = new script_array(ctx);
				arr->set_id(ctx->memory->set(rs_builtin_type::t_array, sizeof(script_array*), arr));
				arr->add_prototype(ctx->array_prototype, false, nullptr, 0);
				state->registers()[i->args[0].reg] = arr->id();
			} else {
				auto arr = new script_array(ctx);
				arr->add_prototype(ctx->array_prototype, false, nullptr, 0);
				arr->set_id(i->args[0].var);
				ctx->memory->set(i->args[0].var, rs_builtin_type::t_array, sizeof(script_array*), arr);
			}
		} else {
			auto arr = new script_array(ctx);
			arr->add_prototype(ctx->array_prototype, false, nullptr, 0);
			arr->set_id(ctx->memory->set(rs_builtin_type::t_array, sizeof(script_array*), arr));
			state->registers()[rs_register::lvalue] = arr->id();
		}
	}
	inline void df_newObj(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		if (i->arg_count > 0) {
			if (i->arg_is_register[0]) {
				auto obj = new script_object(ctx);
				obj->set_id(ctx->memory->set(rs_builtin_type::t_object, sizeof(script_object*), obj));
				state->registers()[i->args[0].reg] = obj->id();
			} else {
				auto obj = new script_object(ctx);
				obj->set_id(i->args[0].var);
				ctx->memory->set(i->args[0].var, rs_builtin_type::t_object, sizeof(script_object*), obj);
			}
		} else {
			auto obj = new script_object(ctx);
			obj->set_id(ctx->memory->set(rs_builtin_type::t_object, sizeof(script_object*), obj));
			state->registers()[rs_register::lvalue] = obj->id();
		}
	}
	inline void df_addProto(execution_state* state, instruction* i) {
		throw runtime_exception(
			format("'%s' is not an object", param_tostring(state->ctx(), state->registers(), i, 0).c_str()),
			state,
			*i
		);
	}
	inline void df_prop(execution_state* state, instruction* i) {
		throw runtime_exception(
			format("'%s' is not an object", param_tostring(state->ctx(), state->registers(), i, 0).c_str()),
			state,
			*i
		);
	}
	inline void df_propAssign(execution_state* state, instruction* i) {
		throw runtime_exception(
			format("'%s' is not an object", param_tostring(state->ctx(), state->registers(), i, 0).c_str()),
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

		registers[rs_register::rvalue] = param_truthiness(ctx, registers, i, 0) || param_truthiness(ctx, registers, i, 1);
	}
	inline void df_and(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		registers[rs_register::rvalue] = param_truthiness(ctx, registers, i, 0) && param_truthiness(ctx, registers, i, 1);
	}
	inline void df_orEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		df_or(state, i);

		variable_id dst = 0;
		if (i->arg_is_register[0]) dst = registers[i->args[0].reg].data.v;
		else dst = i->args[0].var;

		ctx->memory->set(dst, rs_builtin_type::t_bool, sizeof(u8), &registers[rs_register::rvalue].data.b);

	}
	inline void df_andEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		df_and(state, i);

		variable_id dst = 0;
		if (i->arg_is_register[0]) dst = registers[i->args[0].reg].data.v;
		else dst = i->args[0].var;

		ctx->memory->set(dst, rs_builtin_type::t_bool, sizeof(u8), &registers[rs_register::rvalue].data.b);
	}
	inline void df_branch(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		if (param_truthiness(ctx, registers, i, 0)) {
			vstore(ctx, registers[rs_register::instruction_address].data.v, i->args[1].var);
			return;
		}
		vstore(ctx, registers[rs_register::instruction_address].data.v, i->args[2].var);
	}
	inline void df_clearParams(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();
		registers[rs_register::parameter0] = register_type();
		registers[rs_register::parameter1] = register_type();
		registers[rs_register::parameter2] = register_type();
		registers[rs_register::parameter3] = register_type();
		registers[rs_register::parameter4] = register_type();
		registers[rs_register::parameter5] = register_type();
		registers[rs_register::parameter6] = register_type();
		registers[rs_register::parameter7] = register_type();
	}
	inline void df_call(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();
		mem_var func_var;
		if (i->arg_is_register[0]) func_var = ctx->memory->get(registers[i->args[0].reg].data.v);
		else func_var = ctx->memory->get(i->args[0].var);

		script_function* func = nullptr;
		bool is_prototype_constructor = false;
		if (func_var.type == rs_builtin_type::t_class) {
			object_prototype* proto = (object_prototype*)func_var.data;
			func = proto->constructor();
			is_prototype_constructor = true;
		}

		if (!func) func = (script_function*)func_var.data;
		if (func->cpp_callback) {
			script_object* this_obj = nullptr;
			variable_id this_obj_id = registers[rs_register::this_obj].data.v;
			if (this_obj_id != 0) {
				mem_var& obj = ctx->memory->get(this_obj_id);
				if (obj.type == rs_builtin_type::t_object || obj.type == rs_builtin_type::t_array) this_obj = (script_object*)obj.data;
			}

			func_args args;
			args.state = state;
			args.context = ctx;
			args.self = this_obj;
			for (u8 arg = 0;arg < 8;arg++) {
				args.parameters.push(registers[rs_register::parameter0 + arg]);
			}

			registers[rs_register::return_value] = func->cpp_callback(&args);
		} else {
			vstore(ctx, registers[rs_register::return_address].data.v, registers[rs_register::instruction_address].data.v);

			state->push_state();
			state->push_scope();

			vstore(ctx, registers[rs_register::instruction_address].data.v, func->entry_point_id);
		}
	}
	inline void df_jump(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();
		vstore(ctx, registers[rs_register::instruction_address].data.v, i->args[0].var);
	}
	inline void df_ret(execution_state* state, instruction* i) {
		state->pop_scope();
		state->pop_state(rs_register::null_register);
		register_type* registers = state->registers();
		vstore(state->ctx(), registers[rs_register::instruction_address].data.v, registers[rs_register::return_address].data.v);
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

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);
		
		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
		) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) + (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av + bv;
		}
	}
	inline void num_sub(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
		) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) - (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av - bv;
		}
	}
	inline void num_mul(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
		) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) * (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av * bv;
		}
	}
	inline void num_div(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
		) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) / (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av / bv;
		}
	}
	inline void num_mod(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) % (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = (decimal_type)fmod(av, bv);
		}
	}
	inline void num_pow(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = ipow((*(integer_type*)a.data), (*(integer_type*)b.data));
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

			registers[rs_register::rvalue] = (decimal_type)::pow(av, bv);
		}
	}
	inline void num_less(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) < (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av < bv;
		}
	}
	inline void num_greater(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) > (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av > bv;
		}
	}
	inline void num_compare(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) == (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av == bv;
		}
	}

	inline void num_addEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = ctx->memory->get(i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			integer_type& a_i = (*(integer_type*)a.data);
			registers[rs_register::rvalue] = a_i += (*(integer_type*)b.data);
		} else {
			if (a.type != type) {
				integer_type& av = *(integer_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av += bv;
			} else if (b.type != type) {
				decimal_type& av = *(decimal_type*)a.data;
				integer_type bv = *(integer_type*)b.data;

				registers[rs_register::rvalue] = av += bv;
			} else {
				decimal_type& av = *(decimal_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av += bv;
			}
		}
	}
	inline void num_subEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = ctx->memory->get(i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			integer_type& a_i = (*(integer_type*)a.data);
			registers[rs_register::rvalue] = a_i -= (*(integer_type*)b.data);
		} else {
			if (a.type != type) {
				integer_type& av = *(integer_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av -= bv;
			} else if (b.type != type) {
				decimal_type& av = *(decimal_type*)a.data;
				integer_type bv = *(integer_type*)b.data;

				registers[rs_register::rvalue] = av -= bv;
			} else {
				decimal_type& av = *(decimal_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av -= bv;
			}
		}
	}
	inline void num_mulEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = ctx->memory->get(i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			integer_type& a_i = (*(integer_type*)a.data);
			registers[rs_register::rvalue] = a_i *= (*(integer_type*)b.data);
		} else {
			if (a.type != type) {
				integer_type& av = *(integer_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av *= bv;
			} else if (b.type != type) {
				decimal_type& av = *(decimal_type*)a.data;
				integer_type bv = *(integer_type*)b.data;

				registers[rs_register::rvalue] = av *= bv;
			} else {
				decimal_type& av = *(decimal_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av *= bv;
			}
		}
	}
	inline void num_divEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = ctx->memory->get(i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			integer_type& a_i = (*(integer_type*)a.data);
			registers[rs_register::rvalue] = a_i /= (*(integer_type*)b.data);
		} else {
			if (a.type != type) {
				integer_type& av = *(integer_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av /= bv;
			} else if (b.type != type) {
				decimal_type& av = *(decimal_type*)a.data;
				integer_type bv = *(integer_type*)b.data;

				registers[rs_register::rvalue] = av /= bv;
			} else {
				decimal_type& av = *(decimal_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av /= bv;
			}
		}
	}
	inline void num_modEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var& a = ctx->memory->get(i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			integer_type& a_i = (*(integer_type*)a.data);
			registers[rs_register::rvalue] = a_i %= (*(integer_type*)b.data);
		} else {
			if (a.type != type) {
				integer_type& av = *(integer_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				decimal_type result = fmod(av, bv);
				registers[rs_register::rvalue] = result;

				a.type = rs_builtin_type::t_decimal;
				a.size = sizeof(decimal_type);
				(*(decimal_type*)a.data) = result;
			} else if (b.type != type) {
				decimal_type& av = *(decimal_type*)a.data;
				integer_type bv = *(integer_type*)b.data;

				registers[rs_register::rvalue] = av = fmod(av, bv);
			} else {
				decimal_type& av = *(decimal_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av = fmod(av, bv);
			}
		}
	}
	inline void num_powEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var& a = ctx->memory->get(i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			integer_type& a_i = (*(integer_type*)a.data);
			registers[rs_register::rvalue] = a_i = ipow(a_i, (*(integer_type*)b.data));
		} else {
			if (a.type != type) {
				integer_type& av = *(integer_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				decimal_type result = ::pow(av, bv);
				registers[rs_register::rvalue] = result;

				a.type = rs_builtin_type::t_decimal;
				a.size = sizeof(decimal_type);
				(*(decimal_type*)a.data) = result;
			} else if (b.type != type) {
				decimal_type& av = *(decimal_type*)a.data;
				integer_type bv = *(integer_type*)b.data;

				registers[rs_register::rvalue] = av = ::pow(av, bv);
			} else {
				decimal_type& av = *(decimal_type*)a.data;
				decimal_type bv = *(decimal_type*)b.data;

				registers[rs_register::rvalue] = av = ::pow(av, bv);
			}
		}
	}
	inline void num_lessEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) <= (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av <= bv;
		}
	}
	inline void num_greaterEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);
		mem_var b = cast_param(ctx, registers, i, 1);

		if (
			!a.type || !b.type || a.size == 0 || b.size == 0 ||
			a.type > rs_builtin_type::t_decimal || b.type > rs_builtin_type::t_decimal ||
			b.type < rs_builtin_type::t_integer
			) return nan_result(ctx, registers);

		type_id type = max(a.type, b.type);

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue] = (*(integer_type*)a.data) >= (*(integer_type*)b.data);
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

			registers[rs_register::rvalue] = av >= bv;
		}
	}
	inline void num_inc(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);

		if (!a.type || a.size == 0 || a.type > rs_builtin_type::t_decimal) return nan_result(ctx, registers);

		if (a.type == rs_builtin_type::t_integer) registers[rs_register::rvalue] = (*(integer_type*)a.data) += integer_type(1);
		else registers[rs_register::rvalue] = (*(decimal_type*)a.data) += decimal_type(1);
	}
	inline void num_dec(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var a = cast_param(ctx, registers, i, 0);

		if (!a.type || a.size == 0 || a.type > rs_builtin_type::t_decimal) return nan_result(ctx, registers);

		if (a.type == rs_builtin_type::t_integer) registers[rs_register::rvalue] = (*(integer_type*)a.data) -= integer_type(1);
		else registers[rs_register::rvalue] = (*(decimal_type*)a.data) -= decimal_type(1);
	}

	inline void obj_addProto(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		script_object* obj = cast_param<script_object*>(ctx, registers, i, 0);
		object_prototype* proto = cast_param<object_prototype*>(ctx, registers, i, 1);

		obj->add_prototype(proto, false, nullptr, 0);
	}
	inline void obj_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		string propName;
		mem_var b = cast_param(ctx, registers, i, 1);
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

		script_object* obj = cast_param<script_object*>(ctx, registers, i, 0);
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

		if (i->arg_is_register[0]) registers[i->args[0].reg] = prop_id;
		else registers[rs_register::lvalue] = prop_id;
	}
	inline void obj_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		string propName;
		mem_var b = cast_param(ctx, registers, i, 1);
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

		script_object* obj = cast_param<script_object*>(ctx, registers, i, 0);
		variable_id prop_id = obj->property(propName);
		if (prop_id == 0) {
			prop_id = obj->define_property(propName, rs_builtin_type::t_null, 0, nullptr);
		}

		if (i->arg_is_register[0]) registers[i->args[0].reg] = prop_id;
		else registers[rs_register::lvalue] = prop_id;
	}

	inline void str_add(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var;
		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg].data.v : i->args[1].var;
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

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var;
		variable_id b_id = i->arg_is_register[1] ? registers[i->args[1].reg].data.v : i->args[1].var;
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

		variable_id a_id = i->arg_is_register[0] ? registers[i->args[0].reg].data.v : i->args[0].var;
		mem_var a = ctx->memory->get(a_id);
		mem_var b = cast_param(ctx, registers, i, 1);

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

	inline void class_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		string propName;
		mem_var b = cast_param(ctx, registers, i, 1);
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

		object_prototype* proto = cast_param<object_prototype*>(ctx, registers, i, 0);
		variable_id prop_id = proto->static_variable(propName);
		if (prop_id == 0) {
			script_function* func = proto->static_method(propName);
			if (func) {
				prop_id = func->function_id;
			} else {
				throw runtime_exception(
					format("Class has no static property or method named '%s'", propName.c_str()),
					state,
					*i
				);
			}
		}
		
		if (i->arg_is_register[0]) registers[i->args[0].reg] = prop_id;
		else registers[rs_register::lvalue] = prop_id;
	}
	inline void class_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		string propName;
		mem_var b = cast_param(ctx, registers, i, 1);
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

		object_prototype* proto = cast_param<object_prototype*>(ctx, registers, i, 0);
		variable_id prop_id = proto->static_variable(propName);
		if (prop_id == 0) {
			script_function* func = proto->static_method(propName);
			if (func) {
				prop_id = func->function_id;
			} else {
				prop_id = ctx->memory->set(rs_builtin_type::t_null, 0, nullptr);
				proto->static_variable(propName, prop_id);
			}
		}

		if (i->arg_is_register[0]) registers[i->args[0].reg] = prop_id;
		else registers[rs_register::lvalue] = prop_id;
	}

	inline void arr_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var b = cast_param(ctx, registers, i, 1);
		if (b.type != rs_builtin_type::t_integer) {
			obj_prop(state, i);
			return;
		}

		integer_type idx = *(integer_type*)b.data;
		script_array* arr = cast_param<script_array*>(ctx, registers, i, 0);
		if (idx < 0 || idx >= arr->count()) {
			throw runtime_exception(
				format("Array index out of range"),
				state,
				*i
			);
		}

		if (i->arg_is_register[0]) registers[i->args[0].reg] = arr->elements()[idx];
		else registers[rs_register::lvalue] = arr->elements()[idx];
	}
	inline void arr_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		register_type* registers = state->registers();

		mem_var b = cast_param(ctx, registers, i, 1);
		if (b.type != rs_builtin_type::t_integer) {
			obj_propAssign(state, i);
			return;
		}

		integer_type idx = *(integer_type*)b.data;
		script_array* arr = cast_param<script_array*>(ctx, registers, i, 0);
		if (idx < 0 || idx >= arr->count()) {
			throw runtime_exception(
				format("Array index out of range"),
				state,
				*i
			);
		}

		if (i->arg_is_register[0]) registers[i->args[0].reg] = arr->elements()[idx];
		else registers[rs_register::lvalue] = arr->elements()[idx];
	}


	void add_default_instruction_set(context* ctx) {
		ctx->define_instruction(0, rs_instruction::store, df_store);
		ctx->define_instruction(0, rs_instruction::newArr, df_newArr);
		ctx->define_instruction(0, rs_instruction::newObj, df_newObj);
		ctx->define_instruction(0, rs_instruction::prop, df_prop);
		ctx->define_instruction(0, rs_instruction::propAssign, df_propAssign);
		ctx->define_instruction(0, rs_instruction::move, df_move);
		ctx->define_instruction(0, rs_instruction::or, df_or);
		ctx->define_instruction(0, rs_instruction::and, df_and);
		ctx->define_instruction(0, rs_instruction::orEq, df_orEq);
		ctx->define_instruction(0, rs_instruction::andEq, df_andEq);
		ctx->define_instruction(0, rs_instruction::branch, df_branch);
		ctx->define_instruction(0, rs_instruction::clearParams, df_clearParams);
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

	void add_class_instruction_set(context* ctx) {
		ctx->define_instruction(rs_builtin_type::t_class, rs_instruction::prop, class_prop);
		ctx->define_instruction(rs_builtin_type::t_class, rs_instruction::propAssign, class_propAssign);
	}

	void add_array_instruction_set(context* ctx) {
		ctx->define_instruction(rs_builtin_type::t_array, rs_instruction::prop, arr_prop);
		ctx->define_instruction(rs_builtin_type::t_array, rs_instruction::propAssign, arr_propAssign);
	}
};