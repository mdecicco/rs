#pragma once
#include <defs.h>
#include <parse_utils.h>
#include <dynamic_array.hpp>
#include <context_memory.h>
#include <context.h>

namespace rs {
	class context;
	class script_object;
	class execution_state;

	struct func_args {
		script_object* self;
		context* context;
		execution_state* state;
		struct arg {
			context_memory::mem_var var;
			variable_id id;
		};
		dynamic_pod_array<arg> parameters;
	};

	class script_function {
		public:
			script_function(context* ctx, const tokenizer::token& name, variable_id entry_point_id, integer_type instruction_count);
			script_function(context* ctx, const std::string& name, script_function_callback cb);
			~script_function();

			tokenizer::token name;

			integer_type entry_point;
			integer_type exit_point;
			variable_id function_id;
			variable_id entry_point_id;
			dynamic_pod_array<variable_id> declared_vars;
			std::vector<variable> params;

			script_function_callback cpp_callback;

		protected:
			context* m_ctx;
	};
};
