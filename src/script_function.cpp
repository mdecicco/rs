#include <script_function.h>
#include <context.h>
using namespace std;

namespace rs {
	script_function::script_function(context* ctx, const tokenizer::token& _name, variable_id entry_id, integer_type instruction_count) {
		m_ctx = ctx;
		name = _name;
		cpp_callback = nullptr;
		entry_point_id = entry_id;
		entry_point = ctx->memory->get(entry_id);
		exit_point = entry_point + instruction_count;
	}

	script_function::script_function(context* ctx, const string& _name, script_function_callback cb) {
		m_ctx = ctx;
		name = { 0, 0, _name, "internal" };
		cpp_callback = cb;
		entry_point_id = 0;
		entry_point = 0;
		exit_point = 0;
		function_id = ctx->memory->create(rs_variable_flags::f_const | rs_variable_flags::f_external | rs_variable_flags::f_static);
		ctx->memory->get(function_id).set(this);
	}

	script_function::~script_function() {
	}
};
