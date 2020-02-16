#pragma once
#include <compiler.h>
#include <dynamic_array.hpp>
#include <context_memory.h>

namespace rs {
	class execution_state;
	class script_function;

	typedef void (*instruction_callback)(execution_state*, instruction_array::instruction*);

	struct variable {
		variable_id id;
		tokenizer::token name;
		bool is_const;
	};

	class context {
		public:
			context(context_parameters& params);
			context(const context& o) = delete;
			~context();

			bool add_code(const std::string& code);
			bool execute(const std::string& code, context_memory::mem_var& result);
			const context_parameters& params() const { return m_params; }

			struct instruction_set {
				instruction_callback callbacks[rs_instruction::instruction_count];
			};
			void define_instruction(type_id type, rs_instruction instruction, instruction_callback cb);
			inline const instruction_set* get_instruction_set(type_id type) {
				if (m_instruction_sets.size() > type) return m_instruction_sets[type];
				return m_instruction_sets[0];
			}

			void bind_function(const std::string& name, script_function_callback cb);

			instruction_array* instructions;
			script_compiler* compiler;
			context_memory* memory;

			std::vector<script_function*> global_functions;
			std::vector<variable> global_variables;

		protected:
			dynamic_pod_array<instruction_set> m_instruction_sets;
			context_parameters m_params;
	};
};