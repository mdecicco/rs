#include <defs.h>
#include <parse_utils.h>
#include <script_object.h>
#include <script_array.h>
using namespace std;

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

	variable::variable() {
		m_id = 0;
		m_flags = 0;
		m_flags |= u8(rs_builtin_type::t_null);
		m_flags |= (u8(0) << 8);
		m_flags |= (rs_variable_flags::f_undefined << 16);
	}

	variable::variable(u16 flags) {
		m_id = 0;
		m_flags = 0;
		m_flags |= u8(rs_builtin_type::t_null);
		m_flags |= (u8(0) << 8);
		m_flags |= (flags << 16);
	}

	variable::~variable() {
	}

	variable_id variable::persist(context* ctx) {
		if (m_id) return m_id;
		m_id = ctx->memory->create((m_flags >> 16) & 0xFFFF);
		ctx->memory->get(m_id).set(*this);
		return m_id;
	}

	string variable::to_string() const {
		if (is_undefined()) return "undefined";
		else if (is_null()) return "null";

		string val;
		u8 t = type();
		switch (t) {
			case rs::rs_builtin_type::t_string: {
				size_t sz = *(size_t*)m_data;
				char* str = *(char**)(m_data + sizeof(size_t));
				val = std::string(str, sz);
				break;
			}
			case rs::rs_builtin_type::t_integer: {
				val = format("%d", *(integer_type*)m_data);
				break;
			}
			case rs::rs_builtin_type::t_decimal: {
				val = format("%f", *(decimal_type*)m_data);
				break;
			}
			case rs::rs_builtin_type::t_bool: {
				val = format("%s", (*(bool*)m_data) ? "true" : "false");
				break;
			}
			case rs::rs_builtin_type::t_class: {
				val = "<class>";
				break;
			}
			case rs::rs_builtin_type::t_function: {
				val = "<function>";
				break;
			}
			case rs::rs_builtin_type::t_object: {
				val = "<object>";
				break;
				script_object* obj = *(script_object**)m_data;
				auto properties = obj->properties();
				val = "{ ";

				u32 i = 0;
				for (auto& p : properties) {
					if (i > 0) val += ", ";
					val += p.name + ": " + obj->ctx()->memory->get(p.id).to_string();
					i++;
				}

				if (i > 0) val += " ";

				val += "}";

				break;
			}
			case rs::rs_builtin_type::t_array: {
				script_array* arr = (script_array*)m_data;
				auto properties = arr->properties();
				auto elements = arr->elements();
				auto element_count = arr->count();
				val = "[ ";

				u32 i = 0;
				for (i;i < element_count;i++) {
					if (i > 0) val += ", ";
					val += arr->ctx()->memory->get(elements[i]).to_string();
				}

				for (auto& p : properties) {
					if (i > 0) val += ", ";
					val += p.name + ": " + arr->ctx()->memory->get(p.id).to_string();
					i++;
				}

				if (i > 0) val += " ";

				val += "]";

				break;
			}
			default: {
				val = "<scripted type>";
				break;
			}
		}

		return val;
	}
};