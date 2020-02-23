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

	inline variable param(context* ctx, variable* registers, instruction* i, u8 arg_idx) {
		if (i->arg_is_register[arg_idx]) return registers[i->args[arg_idx].reg];
		return ctx->memory->get(i->args[arg_idx].var);
	}

	inline bool param_truthiness(context* ctx, variable* registers, instruction* i, u8 arg_idx) {
		variable v = param(ctx, registers, i, arg_idx);
		for (size_t x = 0;x < v.size();x++) {
			if (((char*)v)[x]) return true;
		}
		return false;
	}

	inline bool param_truthiness(const variable& v) {
		for (size_t x = 0;x < v.size();x++) {
			if (((char*)v)[x]) return true;
		}
		return false;
	}

	inline string param_tostring(context* ctx, variable* registers, instruction* i, u8 arg_idx) {
		return param(ctx, registers, i, arg_idx).to_string();
	}

	inline void vstore(context* ctx, variable_id dst, variable_id src) {
		ctx->memory->get(dst).set(ctx->memory->get(src));
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

	inline void nan_result(context* ctx, variable* registers) {
		registers[rs_register::rvalue] = rs_nan;
	}

	inline void df_store(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable_id dst = i->args[0].var;
		ctx->memory->get(dst).set(registers[i->args[1].reg]);
	}
	inline void df_newArr(execution_state* state, instruction* i) {
		context* ctx = state->ctx();

		auto arr = new script_array(ctx);
		arr->set_id(ctx->memory->create(0));
		ctx->memory->get(arr->id()).set(arr);
		arr->add_prototype(ctx->array_prototype, false, nullptr, 0);
		state->registers()[i->args[0].reg] = arr->id();
	}
	inline void df_newObj(execution_state* state, instruction* i) {
		context* ctx = state->ctx();

		auto obj = new script_object(ctx);
		obj->set_id(ctx->memory->create(0));
		ctx->memory->get(obj->id()).set(obj);
		state->registers()[i->args[0].reg] = obj->id();
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
		variable* registers = state->registers();

		registers[i->args[0].reg] = registers[i->args[1].reg];
	}
	inline void df_or(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		registers[rs_register::rvalue] = param_truthiness(registers[i->args[0].reg]) || param_truthiness(registers[i->args[1].reg]);
	}
	inline void df_and(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		registers[rs_register::rvalue] = param_truthiness(registers[i->args[0].reg]) && param_truthiness(registers[i->args[1].reg]);
	}
	inline void df_orEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		df_or(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);

	}
	inline void df_andEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		df_and(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void df_branch(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		if (param_truthiness(registers[i->args[0].reg])) {
			registers[rs_register::instruction_address].set(ctx->memory->get(i->args[1].var));
			return;
		}
		registers[rs_register::instruction_address].set(ctx->memory->get(i->args[2].var));
	}
	inline void df_clearParams(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();
		registers[rs_register::parameter0] = variable();
		registers[rs_register::parameter1] = variable();
		registers[rs_register::parameter2] = variable();
		registers[rs_register::parameter3] = variable();
		registers[rs_register::parameter4] = variable();
		registers[rs_register::parameter5] = variable();
		registers[rs_register::parameter6] = variable();
		registers[rs_register::parameter7] = variable();
	}
	inline void df_call(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();
		variable func_var;
		if (i->arg_is_register[0]) func_var = registers[i->args[0].reg];
		else func_var = ctx->memory->get(i->args[0].var);

		script_function* func = nullptr;
		bool is_prototype_constructor = false;
		if (func_var.type() == rs_builtin_type::t_function) func = (script_function*)func_var;
		else if (func_var.type() == rs_builtin_type::t_class) {
			object_prototype* proto = (object_prototype*)func_var;
			func = proto->constructor();
			is_prototype_constructor = true;
		}
		else {
			throw runtime_exception(
				format("'%s' is not a function or prototype", func_var.to_string().c_str()),
				state,
				*i
			);
		}
		
		if (func->cpp_callback) {
			func_args args;
			args.state = state;
			args.context = ctx;
			args.self = registers[rs_register::this_obj];
			for (u8 arg = 0;arg < 8;arg++) {
				args.parameters.push(registers[rs_register::parameter0 + arg]);
			}

			registers[rs_register::return_value] = func->cpp_callback(&args);
		} else {
			registers[rs_register::return_address].set(registers[rs_register::instruction_address]);

			state->push_state();
			state->push_scope();

			registers[rs_register::instruction_address].set(func->entry_point);
		}
	}
	inline void df_jump(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();
		registers[rs_register::instruction_address].set(ctx->memory->get(i->args[0].var));
	}
	inline void df_ret(execution_state* state, instruction* i) {
		state->pop_scope();
		state->pop_state(rs_register::null_register);
		variable* registers = state->registers();
		registers[rs_register::instruction_address].set(registers[rs_register::return_address]);
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
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a + (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av + bv);
		}
	}
	inline void num_sub(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a - (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av - bv);
		}
	}
	inline void num_mul(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a * (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av * bv);
		}
	}
	inline void num_div(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a / (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av / bv);
		}
	}
	inline void num_mod(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a % (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set((decimal_type)fmod(av, bv));
		}
	}
	inline void num_pow(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set(ipow((integer_type)a, (integer_type)b));
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set((decimal_type)::pow(av, bv));
		}
	}
	inline void num_less(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a < (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av < bv);
		}
	}
	inline void num_greater(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a > (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av > bv);
		}
	}
	inline void num_compare(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a == (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av == bv);
		}
	}

	inline void num_addEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		num_add(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void num_subEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		num_sub(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void num_mulEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		num_mul(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void num_divEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		num_div(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void num_modEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		num_mod(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void num_powEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		num_pow(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void num_lessEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a <= (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av <= bv);
		}
	}
	inline void num_greaterEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		u8 at = a.type();
		u8 bt = b.type();
		u8 as = a.size();
		u8 bs = b.size();

		if (!at || !bt|| as == 0 || bs == 0 || at > rs_builtin_type::t_decimal || bt > rs_builtin_type::t_decimal || bt < rs_builtin_type::t_integer) return nan_result(ctx, registers);

		type_id type = max(a.type(), b.type());

		if (type == rs_builtin_type::t_integer) {
			registers[rs_register::rvalue].set((integer_type)a >= (integer_type)b);
		} else {
			decimal_type av = 0;
			decimal_type bv = 0;
			if (a.type() != type) {
				av = (integer_type)a;
				bv = (decimal_type)b;
			} else if (b.type() != type) {
				av = (decimal_type)a;
				bv = (integer_type)b;
			} else {
				av = (decimal_type)a;
				bv = (decimal_type)b;
			}

			registers[rs_register::rvalue].set(av >= bv);
		}
	}
	inline void num_inc(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		u8 at = a.type();
		u8 as = a.size();

		if (!at || as == 0 || as > rs_builtin_type::t_decimal) return nan_result(ctx, registers);

		if (at == rs_builtin_type::t_integer) registers[rs_register::rvalue].set((integer_type)a + integer_type(1));
		else registers[rs_register::rvalue].set((decimal_type)a + decimal_type(1));

		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void num_dec(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		u8 at = a.type();
		u8 as = a.size();

		if (!at || as == 0 || as > rs_builtin_type::t_decimal) return nan_result(ctx, registers);

		if (at == rs_builtin_type::t_integer) registers[rs_register::rvalue].set((integer_type)a + integer_type(1));
		else registers[rs_register::rvalue].set((decimal_type)a + decimal_type(1));

		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}

	inline void obj_addProto(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		script_object* obj = registers[i->args[0].reg];
		object_prototype* proto = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);

		obj->add_prototype(proto, false, nullptr, 0);
	}
	inline void obj_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		string propName;
		variable& b = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);
		switch (b.type()) {
			case rs_builtin_type::t_string: {
				propName = b.to_string();
				break;
			}
			case rs_builtin_type::t_integer: {
				propName = format("%d", (integer_type)b);
				break;
			}
			case rs_builtin_type::t_decimal: {
				propName = format("%f", (decimal_type)b);
				break;
			}
			default: {
				throw runtime_exception(
					format("'%s' is not a valid property name or index", b.to_string().c_str()),
					state,
					*i
				);
			}
		}

		script_object* obj = registers[i->args[0].reg];
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

		registers[i->args[0].reg] = ctx->memory->get(prop_id);
	}
	inline void obj_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		string propName;
		variable& b = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);
		switch (b.type()) {
			case rs_builtin_type::t_string: {
				propName = b.to_string();
				break;
			}
			case rs_builtin_type::t_integer: {
				propName = format("%d", (integer_type)b);
				break;
			}
			case rs_builtin_type::t_decimal: {
				propName = format("%f", (decimal_type)b);
				break;
			}
			default: {
				throw runtime_exception(
					format("'%s' is not a valid property name or index", b.to_string().c_str()),
					state,
					*i
				);
			}
		}

		script_object* obj = registers[i->args[0].reg];
		variable_id prop_id = obj->property(propName);
		if (prop_id == 0) prop_id = obj->proto_property(propName);
		if (prop_id == 0) prop_id = obj->define_property(propName, 0);

		registers[i->args[0].reg] = prop_id;
	}

	inline void str_add(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = registers[i->args[1].reg];

		string r = a.to_string() + b.to_string();

		char* result = new char[r.length()];
		memcpy(result, r.c_str(), r.length());
		registers[rs_register::rvalue].set(result, r.length());
	}
	inline void str_addEq(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		str_add(state, i);

		variable& a = registers[i->args[0].reg];
		variable_id dst = a.id();
		if (dst) ctx->memory->get(dst).set(registers[rs_register::rvalue]);
		else a.set(registers[rs_register::rvalue]);
	}
	inline void str_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& a = registers[i->args[0].reg];
		variable& b = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);

		integer_type idx = -1;
		if (b.type() == rs_builtin_type::t_integer) idx = b;
		else {
			throw runtime_exception(
				format("'%s' is not a valid string index", b.to_string().c_str()),
				state,
				*i
			);
			return;
		}

		string s = a.to_string();
		if (idx < 0 || idx >= s.length()) {
			throw runtime_exception(
				format("String index out of range"),
				state,
				*i
			);
			return;
		}

		char* result = new char[1];
		*result = s[0];
		registers[i->args[0].reg].set(result, 1);
	}

	inline void class_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		string propName;
		variable& b = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);
		switch (b.type()) {
			case rs_builtin_type::t_string: {
				propName = b.to_string();
				break;
			}
			case rs_builtin_type::t_integer: {
				propName = format("%d", (integer_type)b);
				break;
			}
			case rs_builtin_type::t_decimal: {
				propName = format("%f", (decimal_type)b);
				break;
			}
			default: {
				throw runtime_exception(
					format("'%s' is not a valid property name or index", b.to_string().c_str()),
					state,
					*i
				);
			}
		}

		object_prototype* proto = registers[i->args[0].reg];
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
		
		registers[i->args[0].reg] = ctx->memory->get(prop_id);
	}
	inline void class_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		string propName;
		variable& b = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);
		switch (b.type()) {
			case rs_builtin_type::t_string: {
				propName = b.to_string();
				break;
			}
			case rs_builtin_type::t_integer: {
				propName = format("%d", (integer_type)b);
				break;
			}
			case rs_builtin_type::t_decimal: {
				propName = format("%f", (decimal_type)b);
				break;
			}
			default: {
				throw runtime_exception(
					format("'%s' is not a valid property name or index", b.to_string().c_str()),
					state,
					*i
				);
			}
		}

		object_prototype* proto = registers[i->args[0].reg];
		variable_id prop_id = proto->static_variable(propName);
		if (prop_id == 0) {
			script_function* func = proto->static_method(propName);
			if (func) {
				prop_id = func->function_id;
			} else {
				prop_id = ctx->memory->create(0);
				proto->static_variable(propName, prop_id);
			}
		}

		registers[i->args[0].reg] = ctx->memory->get(prop_id);
	}

	inline void arr_prop(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& b = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);
		if (b.type() != rs_builtin_type::t_integer) {
			obj_prop(state, i);
			return;
		}

		integer_type idx = (integer_type)b;
		script_array* arr = registers[i->args[0].reg];
		if (idx < 0 || idx >= arr->count()) {
			throw runtime_exception(
				format("Array index out of range"),
				state,
				*i
			);
		}

		registers[i->args[0].reg] = ctx->memory->get(arr->elements()[idx]);
	}
	inline void arr_propAssign(execution_state* state, instruction* i) {
		context* ctx = state->ctx();
		variable* registers = state->registers();

		variable& b = i->arg_is_register[1] ? registers[i->args[1].reg] : ctx->memory->get(i->args[1].var);
		if (b.type() != rs_builtin_type::t_integer) {
			obj_prop(state, i);
			return;
		}

		integer_type idx = (integer_type)b;
		script_array* arr = registers[i->args[0].reg];
		if (idx < 0 || idx >= arr->count()) {
			throw runtime_exception(
				format("Array index out of range"),
				state,
				*i
			);
		}

		registers[i->args[0].reg] = ctx->memory->get(arr->elements()[idx]);
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