#include <script_string.h>
#include <context.h>
#include <prototype.h>
#include <script_function.h>

namespace rs {
	variable_id str_constructor(script_object* thisObj, const func_args* args) {
	}

	void bind_script_string(context* ctx) {
		//object_prototype* proto = new object_prototype("string");
		//proto->constructor(new script_function(ctx, "", str_constructor));
	}
};