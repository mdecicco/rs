#include <prototype.h>
#include <context.h>
#include <script_function.h>
#include <script_object.h>
using namespace std;

namespace rs {
	static type_id next_type_id = rs_builtin_type::script_type_base + 1;

	object_prototype::object_prototype(context* ctx, const string& name) {
		m_id = ctx->memory->create(rs_variable_flags::f_external | rs_variable_flags::f_const | rs_variable_flags::f_static);
		ctx->memory->get(m_id).set(this);
		m_declaration = { 0, 0, name, "internal" };
		m_type_id = next_type_id++;
		m_constructor = nullptr;
		parent = nullptr;
	}

	object_prototype::object_prototype(context* ctx, const tokenizer::token& declaration) {
		m_id = ctx->memory->create(rs_variable_flags::f_const | rs_variable_flags::f_static);
		ctx->memory->get(m_id).set(this);
		m_declaration = declaration;
		m_type_id = next_type_id++;
		m_constructor = nullptr;
		parent = nullptr;
	}

	object_prototype::~object_prototype() {
		m_methods.clear();
		m_static_methods.clear();
		m_static_vars.clear();
	}
	
	script_object* object_prototype::create(context* ctx, variable_id* args, u8 arg_count) {
		script_object* obj = new script_object(ctx);
		obj->set_id(ctx->memory->create(rs_variable_flags::f_external));
		ctx->memory->get(obj->id()).set(obj);
		obj->add_prototype(this, true, args, arg_count);
		return obj;
	}

	script_function* object_prototype::method(const string& name) {
		if (m_methods.has(name)) return *m_methods.get(name);
		if (parent) return parent->method(name);
		return nullptr;
	}

	script_function* object_prototype::static_method(const string& name) {
		if (m_static_methods.has(name)) return *m_static_methods.get(name);
		if (parent) return parent->static_method(name);
		return nullptr;
	}

	variable_id object_prototype::static_variable(const string& name) {
		if (m_static_vars.has(name)) return *m_static_vars.get(name);
		if (parent) return parent->static_variable(name);
		return 0;
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