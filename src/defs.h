#pragma once
#include <stdint.h>

namespace rs {
	enum rs_instruction {
		null_instruction = 0,
		// copy value of thing on right into variable
		// referred to by register on left
		store,
		prop,
		// make register point to variable on right, or variable
		// pointed to by register on right
		move,
		add,
		sub,
		mul,
		div,
		mod,
		pow,
		or,
		and,
		less,
		greater,
		addEq,
		subEq,
		mulEq,
		divEq,
		modEq,
		powEq,
		orEq,
		andEq,
		lessEq,
		greaterEq,
		inc,
		dec,
		branch,
		call,
		ret,
		// child state becomes current, and
		// parent registers copied into it
		pushState,
		// if any registers are supplied as
		// arguments, they will be copied to
		// the parent state. return_value and
		// instruction_address are always
		// persisted
		popState,
		pushScope,
		popScope,
		instruction_count
	};

	enum rs_register {
		null_register = 0,
		this_obj,
		// persisted between states
		return_value,
		return_address,
		// persisted between states
		instruction_address,
		lvalue,
		rvalue,
		parameter0,
		parameter1,
		parameter2,
		parameter3,
		parameter4,
		parameter5,
		parameter6,
		parameter7,
		parameter_count,
		register_count
	};

	enum rs_builtin_type {
		t_int8 = 1,
		t_uint8,
		t_int16,
		t_uint16,
		t_int32,
		t_uint32,
		t_int64,
		t_uint64,
		t_float,
		t_double,
		t_string,
		t_class,
		t_function,
		t_object,
		script_type_base
	};

	typedef uint64_t	register_type;
	typedef uint64_t	variable_id;
	typedef uint64_t	u64;
	typedef int64_t		i64;
	typedef uint32_t	u32;
	typedef int32_t		i32;
	typedef uint16_t	u16;
	typedef uint8_t		u8;
	typedef uint16_t	type_id;

	class context;
	struct context_parameters {
		context* ctx;
		struct {
			size_t resize_amount = 16;
			size_t initial_size = 256;
		} instruction_array;

		struct {
			size_t max_stack_depth = 64;
		} execution;

		struct {
			size_t max_size = 0;
		} memory;
	};
};
