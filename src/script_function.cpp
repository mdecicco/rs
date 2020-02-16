#include <script_function.h>
using namespace std;

namespace rs {
	script_function::script_function(context* ctx, const tokenizer::token& _name, variable_id entry_id) {
		m_ctx = ctx;
		name = _name;
		cpp_callback = nullptr;
		entry_point_id = entry_id;
		entry_point = *(u64*)ctx->memory->get(entry_id).data;
	}

	script_function::script_function(context* ctx, const string& _name, function_callback cb) {
		m_ctx = ctx;
		name = { 0, 0, _name, "internal" };
		cpp_callback = cb;
		entry_point_id = 0;
		entry_point = 0;
	}

	script_function::~script_function() {
	}
};
