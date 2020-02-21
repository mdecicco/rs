#pragma once
#include <defs.h>

namespace rs {
	class context_memory {
		public:
			context_memory(const context_parameters& params);
			~context_memory();

			static variable_id gen_var_id() { return next_var_id++; }

			void set(variable_id id, type_id type, size_t size, void* data);
			variable_id set(type_id type, size_t size, void* data);
			variable_id set_static(type_id type, size_t size, void* data);
			variable_id inject(type_id type, size_t size, void* ptr);
			mem_var& get(variable_id id);

			// callback signature MUST match 'bool (*)(const mem_var&)'
			template <typename F>
			variable_id find_static(F&& find_callback) const {
				for (auto& v : m_static_vars) {
					if (find_callback(v.second)) return v.first;
				}
				return 0;
			}

			void deallocate(variable_id id);

		protected:
			size_t m_max_memory;

			munordered_map<variable_id, mem_var> m_vars;
			munordered_map<variable_id, mem_var> m_static_vars;

			static variable_id next_var_id;
			static mem_var null;
	};

	std::string var_tostring(mem_var& v);
};