#include <script_array.h>
#include <script_function.h>
#include <prototype.h>

namespace rs {
	script_array::script_array(context* ctx) : script_object(ctx) {
	}

	script_array::~script_array() {
	}

	void script_array::push(variable& value) {
		m_elements.push(value.persist(m_context));
	}



	variable array_push(func_args* args) {
		script_array* arr = (script_array*)args->self;
		args->parameters.for_each([&arr](variable* arg) {
			if (arg->is_null() || arg->is_undefined()) return true;
			arr->push(*arg);
			return true;
		});
		return args->self;
	}

	void initialize_array_prototype(context* ctx, object_prototype* proto) {
		proto->method(new script_function(ctx, "push", array_push));
	}
};