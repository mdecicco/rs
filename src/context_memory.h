#pragma once
#include <defs.h>
#include <dynamic_array.hpp>

namespace rs {
	class context_memory {
		public:
			context_memory(const context_parameters& params);
			~context_memory();

			variable_id create(u16 flags);
			inline bool check(variable_id id) const { return id >= m_vars.size(); }
			inline variable& get(variable_id id) { return *m_vars[id]; }

			void destroy(variable_id id);

			// callback signature MUST match 'bool (*)(const variable&)'
			template <typename F>
			variable_id find_static(F&& find_callback) {
				variable_id vid = 0;
				bool found = false;
				m_vars.for_each([&vid, &found, &find_callback](variable* var) {
					if (var->is_static() && find_callback(*var)) {
						found = true;
						return false;
					}
					vid++;
					return true;
				});
				return found ? vid : 0;
			}

		protected:
			size_t m_max_memory;

			dynamic_pod_array<variable> m_vars;
			dynamic_pod_array<variable_id> m_free_slots;
	};
};