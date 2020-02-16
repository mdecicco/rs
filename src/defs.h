#pragma once
#include <stdint.h>

namespace rs {
	enum rs_instruction {
		null_instruction = 0,
		// copy value of thing on right into variable
		// referred to by register on left
		store,
		newObj,
		prop,
		propAssign,
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
		compare,
		inc,
		dec,
		branch,
		call,
		jump,
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
		// persisted only to new state, not back from old state
		this_obj,
		// persisted only from old state, not to new state
		return_value,
		// set to rs_integer_max for new states
		return_address,
		// persisted from old state and to new state
		instruction_address,
		// persisted only to new state, not back from old state
		lvalue,
		// persisted only to new state, not back from old state
		rvalue,
		// persisted only to new state, not back from old state
		parameter0,
		// persisted only to new state, not back from old state
		parameter1,
		// persisted only to new state, not back from old state
		parameter2,
		// persisted only to new state, not back from old state
		parameter3,
		// persisted only to new state, not back from old state
		parameter4,
		// persisted only to new state, not back from old state
		parameter5,
		// persisted only to new state, not back from old state
		parameter6,
		// persisted only to new state, not back from old state
		parameter7,
		// persisted only to new state, not back from old state
		parameter_count,
		register_count
	};

	enum rs_builtin_type {
		t_null,
		t_integer,
		t_decimal,
		t_bool,
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

	inline bool type_is_ptr(type_id type) {
		return 
			type == rs_builtin_type::t_object
			|| type == rs_builtin_type::t_function;
	}

	#ifdef SCRIPTS_USE_64BIT_INTEGERS
		typedef i64	integer_type;
		#define rs_integer_max INT64_MAX
		#define rs_integer_min INT64_MIN
	#else
		typedef i32 integer_type;
		#define rs_integer_max INT_MAX
		#define rs_integer_min INT_MIN
	#endif

	#ifdef SCRIPTS_USE_64BIT_DECIMALS
			typedef double	decimal_type;
			#define rs_nan (::nan(""))
			#define rs_decimal_max DBL_MAX
			#define rs_decimal_min DBL_MIN
	#else
			typedef float decimal_type;
			#define rs_nan (::nanf(""))
			#define rs_decimal_max FLT_MAX
			#define rs_decimal_min FLT_MIN
	#endif

	extern const size_t max_variable_size;


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
