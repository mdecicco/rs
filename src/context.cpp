#include <context.h>
#include <instruction_sets.h>
#include <execution_state.h>
using namespace std;

namespace rs {
	variable_id context_memory::next_var_id = 1;
	context_memory::mem_var context_memory::null = { nullptr, 0, 0 };

	context_memory::context_memory(const context_parameters& params) {
	}

	context_memory::~context_memory() {
		for (auto& v : m_vars) {
			if (v.second.data) delete [] v.second.data;
		}
		m_vars.clear();

		for (auto& v : m_static_vars) {
			if (v.second.data) delete [] v.second.data;
		}
		m_static_vars.clear();
	}

	void context_memory::set(variable_id id, type_id type, size_t size, void* data) {
		if (m_vars.count(id) == 0) {
			u8* d = new u8[size];
			memcpy(d, data, size);
			m_vars[id] = { d, size, type };
		} else {
			auto& v = m_vars[id];
			if (v.size == size) memcpy(v.data, data, size);
			else {
				delete [] v.data;
				v.data = new u8[size];
				memcpy(v.data, data, size);
				v.size = size;
				v.type = type;
			}
		}
	}

	variable_id context_memory::set(type_id type, size_t size, void* data) {
		u8* d = new u8[size];
		memcpy(d, data, size);
		variable_id id = gen_var_id();
		m_vars[id] = { d, size, type };
		return id;
	}

	variable_id context_memory::set_static(type_id type, size_t size, void* data) {
		u8* d = new u8[size];
		memcpy(d, data, size);
		variable_id id = gen_var_id();
		mem_var v = { d, size, type };
		m_static_vars[id] = v;
		return id;
	}

	context_memory::mem_var& context_memory::get(variable_id id) {
		if (m_vars.count(id) != 0) return m_vars[id];
		if (m_static_vars.count(id) != 0) return m_static_vars[id];
		return null;
	}

	void context_memory::deallocate(variable_id id) {
		if (m_vars.count(id) == 0) {
			printf("Var doesn't exist\n");
		} else {
			auto& v = m_vars[id];
			if (v.data) delete [] v.data;
			m_vars.erase(id);
		}
	}


	function::function(context* ctx, const tokenizer::token& _name, variable_id entry_id) {
		m_ctx = ctx;
		name = _name;
		entry_point_id = entry_id;
		entry_point = *(u64*)ctx->memory->get(entry_id).data;
	}

	function::~function() {
	}


	context::context(context_parameters& params) {
		params.ctx = this;
		instructions = new instruction_array(params);
		compiler = new script_compiler(params);
		memory = new context_memory(params);

		add_default_instruction_set(this);
		add_i32_instruction_set(this);
		memcpy(&m_params, &params, sizeof(context_parameters));
	}

	context::~context() {
		delete instructions;
		delete compiler;
		delete memory;
	}

	void context::add_code(const string& code) {
		compiler->compile(code, *instructions);
	}

	void context::execute(const string& code, context_memory::mem_var& result) {
		u64 entry = instructions->count();
		if (compiler->compile(code, *instructions)) {
			execution_state es(m_params, this);
			es.execute_all(entry);
			variable_id ret_id = es.registers()[rs_register::rvalue];
			auto& ret = memory->get(ret_id);
			if (ret.size) {
				result.type = ret.type;
				result.size = ret.size;
				result.data = new u8[ret.size];
				memcpy(result.data, ret.data, ret.size);
			}
		}
	}

	void context::define_instruction(type_id type, rs_instruction instruction, instruction_callback cb) {
		while (m_instruction_sets.size() <= type) {
			instruction_set set;
			memset(&set, 0, sizeof(instruction_set));
			m_instruction_sets.push(set);
		}

		m_instruction_sets[type]->callbacks[instruction] = cb;
	}
};