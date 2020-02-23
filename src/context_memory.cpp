#include <context_memory.h>
#include <execution_state.h>
#include <script_object.h>
#include <script_array.h>
#include <parse_utils.h>

namespace rs {
	context_memory::context_memory(const context_parameters& params) {
		m_max_memory = params.memory.max_size;
	}

	context_memory::~context_memory() {
	}

	variable_id context_memory::create(u16 flags) {
		if (m_free_slots.size() > 0) {
			variable_id id = *m_free_slots[m_free_slots.size() - 1];
			m_free_slots.remove(m_free_slots.size() - 1);
			return id;
		}
		m_vars.construct(flags);
		m_vars[m_vars.size() - 1]->m_id = m_vars.size() - 1;
		return m_vars.size() - 1;
	}

	void context_memory::destroy(variable_id id) {
		m_free_slots.push(id);
	}
};