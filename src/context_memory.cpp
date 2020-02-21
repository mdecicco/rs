#include <context_memory.h>
#include <execution_state.h>
#include <script_object.h>
#include <script_array.h>
#include <parse_utils.h>

namespace rs {
	mem_var context_memory::null = { nullptr, 0, 0, true };
	variable_id context_memory::next_var_id = 1;

	context_memory::context_memory(const context_parameters& params) {
		m_max_memory = params.memory.max_size;
	}

	context_memory::~context_memory() {
		for (auto& v : m_vars) {
			if (v.second.data && !v.second.external) {
				if (type_is_ptr(v.second.type)) delete v.second.data;
				else delete [] v.second.data;
			}
		}
		m_vars.clear();

		for (auto& v : m_static_vars) {
			if (v.second.data && !v.second.external) {
				if (type_is_ptr(v.second.type)) delete v.second.data;
				else delete [] v.second.data;
			}
		}
		m_static_vars.clear();
	}

	void context_memory::set(variable_id id, type_id type, size_t size, void* data) {
		if (m_vars.count(id) == 0) {
			u8* d = nullptr;
			if (type_is_ptr(type)) d = (u8*)data;
			else {
				d = new u8[max_variable_size];
				if (size > 0) memcpy(d, data, size);
			}
			m_vars[id] = { d, size, type, false };
		} else {
			auto& v = m_vars[id];

			if (type_is_ptr(type)) {
				if (type_is_ptr(v.type)) {
					if (v.type == rs_builtin_type::t_string) delete [] *(char**)v.data;
					v.data = (u8*)data;
				}
				else {
					delete [] v.data;
					v.data = (u8*)data;
				}
			} else {
				if (type_is_ptr(v.type)) v.data = new u8[max_variable_size];
				if (size > 0) memcpy(v.data, data, size);
			}

			v.size = size;
			v.type = type;
		}
	}

	variable_id context_memory::set(type_id type, size_t size, void* data) {
		u8* d = nullptr;
		if (type_is_ptr(type)) d = (u8*)data;
		else {
			d = new u8[max_variable_size];
			if (size > 0) memcpy(d, data, size);
		}
		variable_id id = gen_var_id();
		m_vars[id] = { d, size, type, false };
		return id;
	}

	variable_id context_memory::set_static(type_id type, size_t size, void* data) {
		u8* d = nullptr;
		if (type_is_ptr(type)) d = (u8*)data;
		else {
			d = new u8[max_variable_size];
			if (size > 0) memcpy(d, data, size);
		}

		variable_id id = gen_var_id();
		mem_var v = { d, size, type, true };
		m_static_vars[id] = v;
		return id;
	}

	variable_id context_memory::inject(type_id type, size_t size, void* ptr) {
		variable_id id = gen_var_id();
		mem_var v = { ptr, size, type, true };
		m_vars[id] = v;
		return id;
	}
	
	variable_id context_memory::copy(variable_id id) {
		if (m_vars.count(id) != 0) {
			mem_var v = m_vars[id];
			return set(v.type, v.size, v.data);
		}
		if (m_static_vars.count(id) != 0) {
			mem_var v = m_static_vars[id];
			return set(v.type, v.size, v.data);
		}
		return 0;
	}

	mem_var& context_memory::get(variable_id id) {
		if (m_vars.count(id) != 0) return m_vars[id];
		if (m_static_vars.count(id) != 0) return m_static_vars[id];

		return null;
	}

	void context_memory::deallocate(variable_id id) {
		if (m_vars.count(id) == 0) {
			throw runtime_exception(format("Variable %llu doesn't exist", id));
		} else {
			auto& v = m_vars[id];
			if (v.data) delete [] v.data;
			m_vars.erase(id);
		}
	}


	std::string var_tostring(rs::mem_var& v) {
		std::string val;
		if (!v.data) val = "undefined";
		else {
			switch (v.type) {
				case rs::rs_builtin_type::t_string: {
					val = std::string((char*)v.data, v.size);
					break;
				}
				case rs::rs_builtin_type::t_integer: {
					val = rs::format("%d", *(integer_type*)v.data);
					break;
				}
				case rs::rs_builtin_type::t_decimal: {
					val = rs::format("%f", *(decimal_type*)v.data);
					break;
				}
				case rs::rs_builtin_type::t_bool: {
					val = rs::format("%s", (*(bool*)v.data) ? "true" : "false");
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
					script_object* obj = (script_object*)v.data;
					auto properties = obj->properties();
					val = "{ ";

					u32 i = 0;
					for (auto& p : properties) {
						if (i > 0) val += ", ";
						val += p.name + ": " + var_tostring(obj->ctx()->memory->get(p.id));
						i++;
					}

					if (i > 0) val += " ";

					val += "}";

					break;
				}
				case rs::rs_builtin_type::t_array: {
					script_array* arr = (script_array*)v.data;
					auto properties = arr->properties();
					auto elements = arr->elements();
					auto element_count = arr->count();
					val = "[ ";

					u32 i = 0;
					for (i;i < element_count;i++) {
						if (i > 0) val += ", ";
						val += var_tostring(arr->ctx()->memory->get(elements[i]));
					}

					for (auto& p : properties) {
						if (i > 0) val += ", ";
						val += p.name + ": " + var_tostring(arr->ctx()->memory->get(p.id));
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
		}

		return val;
	}
};