#include <prototype.h>
#include <context.h>
#include <script_function.h>
using namespace std;

namespace rs {
	static type_id next_type_id = rs_builtin_type::script_type_base + 1;

	object_prototype::object_prototype(const string& name) {
		m_name = name;
		m_type_id = next_type_id++;
		m_constructor = nullptr;
	}

	object_prototype::~object_prototype() {
		m_methods.clear();
		m_static_methods.clear();
		m_static_vars.clear();
	}

	script_function* object_prototype::method(const string& name) {
		if (m_methods.has(name)) return *m_methods.get(name);
	}

	script_function* object_prototype::static_method(const string& name) {
		if (m_static_methods.has(name)) return *m_static_methods.get(name);
	}

	variable_id object_prototype::static_variable(const string& name) {
		if (m_static_vars.has(name)) return *m_static_vars.get(name);
	}

	object_prototype* object_prototype::constructor(script_function* func) {
		m_constructor = func;
		return this;
	}

	object_prototype* object_prototype::method(script_function* func) {
		m_methods.set(func->name.text, func);
		return this;
	}

	object_prototype* object_prototype::static_method(script_function* func) {
		m_static_methods.set(func->name.text, func);
		return this;
	}

	object_prototype* object_prototype::static_variable(const string& name, variable_id id) {
		m_static_vars.set(name, id);
		return this;
	}
};