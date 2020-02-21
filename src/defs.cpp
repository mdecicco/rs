#include <defs.h>
#include <script_object.h>
#define max(a, b) ((a) > (b) ? (a) : (b))
namespace rs {
	const size_t max_variable_size = max(
		sizeof(script_object*),
		max(
			sizeof(integer_type),
			sizeof(decimal_type)
		)
	);

	const size_t type_sizes[] = {
		0,
		sizeof(integer_type),
		sizeof(decimal_type),
		sizeof(u8),
		sizeof(char*),
		sizeof(object_prototype*),
		sizeof(script_function*),
		sizeof(script_object*),
		sizeof(object_prototype*)
	};


	register_type::register_type() {
		type = rs_builtin_type::t_null;
		data.v = 0;
	}
	register_type::register_type(variable_id v) {
		type = rs_builtin_type::t_null;
		data.v = v;
	}
	register_type::register_type(integer_type i) {
		type = rs_builtin_type::t_integer;
		data.i = i;
	}
	register_type::register_type(decimal_type d) {
		type = rs_builtin_type::t_decimal;
		data.d = d;
	}
	register_type::register_type(bool b) {
		type = rs_builtin_type::t_bool;
		data.b = b;
	}
	/*
	register_type::operator variable_id&() { return data.v; }
	register_type::operator integer_type&() { return data.i; }
	register_type::operator decimal_type&() { return data.d; }
	register_type::operator u8&() { return data.b; }
	*/

	void register_type::copy(context* ctx, size_t* size, type_id* _type, void** _data) {
		switch (type) {
			case rs_builtin_type::t_null:
				if (data.v == 0) {
					*size = 0;
					*_type = type;
					*_data = nullptr;
					return;
				}
				mem_var v = ctx->memory->get(data.v);
				*size = v.size;
				*_type = v.type;
				*_data = nullptr;
				if (v.size) {
					*_data = new u8[v.size];
					memcpy(*_data, v.data, v.size);
				}
				return;
			case rs_builtin_type::t_integer:
				*size = sizeof(integer_type);
				*_type = type;
				*_data = new u8[*size];
				memcpy(*_data, &data, *size);
				return;
			case rs_builtin_type::t_decimal:
				*size = sizeof(decimal_type);
				*_type = type;
				*_data = new u8[*size];
				memcpy(*_data, &data, *size);
				return;
			case rs_builtin_type::t_bool:
				*size = sizeof(u8);
				*_type = type;
				*_data = new u8[*size];
				((u8*)*_data)[0] = data.b;
				return;
		}
	}

	mem_var register_type::ref(context* ctx) {
		if (type == rs_builtin_type::t_null) return ctx->memory->get(data.v);
		mem_var v;
		v.data = &data;
		v.external = false;
		v.size = type_sizes[type];
		v.type = type;
		return v;
	}
};