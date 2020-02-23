#pragma once
#include <stdint.h>
#include <robin_hood.h>
#include <string>

#define mvector std::vector
#define munordered_map robin_hood::unordered_map

namespace rs {
	enum rs_instruction {
		null_instruction = 0,
		load,
		store,
		newArr,
		newObj,
		addProto,
		prop,
		propAssign,
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
		clearParams,
		call,
		jump,
		ret,
		pushState,
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
		t_array,
		script_type_base
	};

	typedef uint64_t	variable_id;
	typedef uint64_t	u64;
	typedef int64_t		i64;
	typedef uint32_t	u32;
	typedef int32_t		i32;
	typedef uint16_t	u16;
	typedef uint8_t		u8;
	typedef uint8_t		type_id;

	inline bool type_is_ptr(type_id type) {
		return 
			   type == rs_builtin_type::t_string
			|| type == rs_builtin_type::t_class
			|| type == rs_builtin_type::t_function
			|| type == rs_builtin_type::t_object
			|| type == rs_builtin_type::t_array;
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

	enum rs_variable_flags {
		f_read_only = 1UL << 16,
		f_static	= 1UL << 17,
		f_const		= 1UL << 18,
		f_null		= 1UL << 19,
		f_undefined	= 1UL << 20,
		f_external	= 1UL << 21,
		f_none09	= 1UL << 22,
		f_none08	= 1UL << 23,
		f_none07	= 1UL << 24,
		f_none06	= 1UL << 25,
		f_none05	= 1UL << 26,
		f_none04	= 1UL << 27,
		f_none03	= 1UL << 28,
		f_none02	= 1UL << 29,
		f_none01	= 1UL << 30,
		f_none00	= 1UL << 31
	};

	class script_array;
	class script_function;
	class object_prototype;
	class script_object;
	class context;

	struct variable {
		variable();
		variable(u16 flags);
		~variable();

		inline void set(const variable& val) {
			memcpy(m_data, val.m_data, val.size());
			m_flags = val.m_flags;
		}
		inline void set(integer_type val) {
			m_flags |= (u8(rs_builtin_type::t_integer) << 0);
			m_flags |= (u8(sizeof(integer_type)) << 8);
			memcpy(m_data, &val, sizeof(integer_type));
		}
		inline void set(decimal_type val) {
			m_flags |= (u8(rs_builtin_type::t_decimal) << 0);
			m_flags |= (u8(sizeof(decimal_type)) << 8);
			memcpy(m_data, &val, sizeof(decimal_type));
		}
		inline void set(char* val, size_t length) {
			m_flags |= (u8(rs_builtin_type::t_string) << 0);
			m_flags |= (u8(sizeof(char*) + sizeof(size_t)) << 8);
			memcpy(m_data, &length, sizeof(size_t));
			memcpy(m_data + sizeof(size_t), &val, sizeof(char*));
			// biggest value, size_t (8 bytes) + pointer (8 bytes)
		}
		inline void set(bool val) {
			m_flags |= (u8(rs_builtin_type::t_bool) << 0);
			m_flags |= (u8(sizeof(u8)) << 8);
			memcpy(m_data, &val, sizeof(u8));
		}
		inline void set(script_object* val) {
			m_flags |= (u8(rs_builtin_type::t_object) << 0);
			m_flags |= (u8(sizeof(script_object*)) << 8);
			memcpy(m_data, &val, sizeof(script_object*));
		}
		inline void set(script_array* val) {
			m_flags |= (u8(rs_builtin_type::t_array) << 0);
			m_flags |= (u8(sizeof(script_array*)) << 8);
			memcpy(m_data, &val, sizeof(script_array*));
		}
		inline void set(script_function* val) {
			m_flags |= (u8(rs_builtin_type::t_function) << 0);
			m_flags |= (u8(sizeof(script_function*)) << 8);
			memcpy(m_data, &val, sizeof(script_function*));
		}
		inline void set(object_prototype* val) {
			m_flags |= (u8(rs_builtin_type::t_class) << 0);
			m_flags |= (u8(sizeof(object_prototype*)) << 8);
			memcpy(m_data, &val, sizeof(object_prototype*));
		}

		inline variable_id id   () const { return m_id; }
		inline u8	type		() const { return (m_flags >> 0) & 0xFF; }
		inline u8	size		() const { return (m_flags >> 8) & 0xFF; }
		inline bool is_read_only() const { return m_flags & rs_variable_flags::f_read_only; }
		inline bool is_static	() const { return m_flags & rs_variable_flags::f_static	  ; }
		inline bool is_const	() const { return m_flags & rs_variable_flags::f_const	  ; }
		inline bool is_null		() const { return m_flags & rs_variable_flags::f_null	  ; }
		inline bool is_undefined() const { return m_flags & rs_variable_flags::f_undefined; }
		inline bool is_external	() const { return m_flags & rs_variable_flags::f_external ; }
		inline bool set_flag(rs_variable_flags flag, bool value) { m_flags ^= (-value ^ m_flags) & flag; }

		inline operator integer_type&		() { return *(     integer_type*)m_data; }
		inline operator decimal_type&		() { return *(     decimal_type*)m_data; }
		inline operator char*				() { return *(            char**)m_data; }
		inline operator bool&				() { return *(             bool*)m_data; }
		inline operator script_object*		() { return *(   script_object**)m_data; }
		inline operator script_array*		() { return *(    script_array**)m_data; }
		inline operator script_function*	() { return *( script_function**)m_data; }
		inline operator object_prototype*	() { return *(object_prototype**)m_data; }

		inline operator integer_type		() const { return *(     integer_type*)m_data; }
		inline operator decimal_type		() const { return *(     decimal_type*)m_data; }
		inline operator char*				() const { return *(            char**)m_data; }
		inline operator bool				() const { return *(             bool*)m_data; }
		inline operator script_object*		() const { return *(   script_object**)m_data; }
		inline operator script_array*		() const { return *(    script_array**)m_data; }
		inline operator script_function*	() const { return *( script_function**)m_data; }
		inline operator object_prototype*	() const { return *(object_prototype**)m_data; }

		variable_id persist(context* ctx);

		std::string to_string() const;

		protected:
			friend class context_memory;
			variable_id m_id;
			u8 m_data[16];
			u32 m_flags;
	};

	extern const size_t max_variable_size;
	extern const size_t type_sizes[];


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

	struct func_args;
	typedef variable (*script_function_callback)(func_args*);
};
