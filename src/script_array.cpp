#include <script_array.h>
#include <script_function.h>
#include <prototype.h>

namespace rs {
	script_array::script_array(context* ctx) : script_object(ctx) {
	}

	script_array::~script_array() {
	}

	void script_array::push(register_type& value) {
		m_elements.push(m_context->memory->copy(value.persist(m_context)));
	}



	variable_id array_push(func_args* args) {
		script_array* arr = (script_array*)args->self;
		args->parameters.for_each([&arr](register_type* arg) {
			if (arg->type == rs_builtin_type::t_null && arg->data.v == 0) return true;
			arr->push(*arg);
			return true;
		});
		return arr->id();
	}

	void initialize_array_prototype(context* ctx, object_prototype* proto) {
		proto->method(new script_function(ctx, "push", array_push));
	}
};