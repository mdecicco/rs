#include <context.h>
#include <instruction_sets.h>
#include <execution_state.h>
#include <script_object.h>
#include <script_function.h>
using namespace std;

namespace rs {
	context::context(context_parameters& params) {
		params.ctx = this;
		instructions = new instruction_array(params);
		compiler = new script_compiler(params);
		memory = new context_memory(params);

		add_default_instruction_set(this);
		add_number_instruction_set(this);
		add_object_instruction_set(this);
		add_string_instruction_set(this);
		memcpy(&m_params, &params, sizeof(context_parameters));
	}

	context::~context() {
		delete instructions;
		delete compiler;
		delete memory;
	}

	bool context::add_code(const string& code) {
		return compiler->compile(code, *instructions);
	}

	bool context::execute(const string& code, context_memory::mem_var& result) {
		u64 entry = instructions->count();
		if (compiler->compile(code, *instructions)) {
			try {
				execution_state es(m_params, this);
				es.execute_all(entry);
				variable_id ret_id = es.registers()[rs_register::rvalue];
				auto& ret = memory->get(ret_id);
				if (ret.size) {
					result.type = ret.type;
					result.size = ret.size;
					if (type_is_ptr(ret.type)) result.data = ret.data;
					else {
						result.data = new u8[ret.size];
						memcpy(result.data, ret.data, ret.size);
					}
				}
				return true;
			} catch (const runtime_exception& e) {
				if (e.has_source_info) {
					printf("%s:%d:%d: %s\n%s\n", e.file.c_str(), e.line + 1, e.col + 1, e.text.c_str(), e.lineText.c_str());
					for (i64 c = 0;c < i64(e.col);c++) printf(" ");
					printf("^\n");
				} else printf("%s\n", e.text.c_str());
				memset(&result, 0, sizeof(context_memory::mem_var));
			}
		}

		return false;
	}

	void context::define_instruction(type_id type, rs_instruction instruction, instruction_callback cb) {
		while (m_instruction_sets.size() <= type) {
			instruction_set set;
			memset(&set, 0, sizeof(instruction_set));
			m_instruction_sets.push(set);
		}

		m_instruction_sets[type]->callbacks[instruction] = cb;
	}

	void context::bind_function(const string& name, script_function_callback cb) {
		auto f = new script_function(this, name, cb);
		f->function_id = memory->set_static(rs_builtin_type::t_function, sizeof(script_function*), f);
		global_functions.push_back(f);
	}
};