#pragma once
#include <context.h>
#include <defs.h>
#include <parse_utils.h>
#include <dynamic_array.hpp>

namespace rs {
	class context;
	class script_object;

	typedef dynamic_pod_array<context_memory::mem_var> func_args;
	typedef variable_id (*function_callback)(script_object*, const func_args*);

	class script_function {
		public:
			script_function(context* ctx, const tokenizer::token& name, variable_id entry_point_id);
			script_function(context* ctx, const std::string& name, function_callback cb);
			~script_function();

			tokenizer::token name;
			u64 entry_point;
			variable_id function_id;
			variable_id entry_point_id;
			dynamic_pod_array<variable_id> declared_vars;
			std::vector<variable> params;
			function_callback cpp_callback;

		protected:
			context* m_ctx;
	};
};
