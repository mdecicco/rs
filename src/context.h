#pragma once
#include <compiler.h>
#include <dynamic_array.hpp>

namespace rs {
	class execution_state;
	class context_memory {
		public:
			context_memory(const context_parameters& params);
			~context_memory();

			static variable_id gen_var_id() { return next_var_id++; }

			struct mem_var {
				void* data;
				size_t size;
				type_id type;
				bool external;
			};

			void set(variable_id id, type_id type, size_t size, void* data);
			variable_id set(type_id type, size_t size, void* data);
			variable_id set_static(type_id type, size_t size, void* data);
			variable_id inject(type_id type, size_t size, void* ptr);
			mem_var& get(variable_id id);

			void deallocate(variable_id id);

		protected:
			size_t m_max_memory;

			std::unordered_map<variable_id, mem_var> m_vars;
			std::unordered_map<variable_id, mem_var> m_static_vars;

			static variable_id next_var_id;
			static mem_var null;
	};

	typedef void (*instruction_callback)(execution_state*, instruction_array::instruction*);

	class context;

	struct variable {
		variable_id id;
		tokenizer::token name;
		bool is_const;
	};

	class function {
		public:
			function(context* ctx, const tokenizer::token& name, variable_id entry_point_id);
			~function();

			tokenizer::token name;
			u64 entry_point;
			variable_id entry_point_id;
			dynamic_pod_array<variable_id> declared_vars;
			std::vector<variable> params;

		protected:
			context* m_ctx;
	};

	class context {
		public:
			context(context_parameters& params);
			context(const context& o) = delete;
			~context();

			void add_code(const std::string& code);
			void execute(const std::string& code, context_memory::mem_var& result);
			const context_parameters& params() const { return m_params; }

			struct instruction_set {
				instruction_callback callbacks[rs_instruction::instruction_count];
			};
			void define_instruction(type_id type, rs_instruction instruction, instruction_callback cb);
			inline const instruction_set* get_instruction_set(type_id type) {
				if (m_instruction_sets.size() > type) return m_instruction_sets[type];
				return m_instruction_sets[0];
			}

			instruction_array* instructions;
			script_compiler* compiler;
			context_memory* memory;

			std::vector<function*> global_functions;
			std::vector<variable> global_variables;

		protected:
			dynamic_pod_array<instruction_set> m_instruction_sets;
			context_parameters m_params;
	};
};