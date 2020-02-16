#include <script_object.h>
#include <execution_state.h>
#include <prototype.h>
using namespace std;

namespace rs {
	script_object::script_object(context* ctx) {
		m_context = ctx;
		m_id = 0;
	}

	script_object::~script_object() {
		m_props.clear();
	}

	variable_id script_object::inject_property(const string& name, type_id type, size_t size, void* ptr) {
		variable_id id = m_context->memory->inject(type, size, ptr);
		m_props[name] = id;
		return id;
	}

	variable_id script_object::define_property(const string& name, type_id type, size_t size, void* data) {
		variable_id id = m_context->memory->set(type, size, data);
		m_props[name] = id;
		return id;
	}

	void script_object::delete_property(const string& name) {
		auto& p = m_props.find(name);
		if (p == m_props.end()) {
			// throw runtime exception
			return;
		}

		m_props.erase(p);
	}

	variable_id script_object::property(const string& name) {
		auto& p = m_props.find(name);
		if (p == m_props.end()) {
			return 0;
		}

		return p->second;
	}

	void script_object::add_prototype(object_prototype* prototype, bool call_constructor, variable_id* args, u8 arg_count) {
		prototypes.push(prototype);
		if (call_constructor) {
			auto constructor = prototype->constructor();
			if (constructor) {
				context_memory::mem_var result;
				m_context->call_function(constructor, m_id, args, arg_count, result);
			}
		}
	}

	vector<script_object::prop> script_object::properties() const {
		vector<prop> props;
		for (auto& p : m_props) props.push_back({ p.second, p.first });
		return props;
	}
};