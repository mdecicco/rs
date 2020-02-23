#pragma once
#include <defs.h>
#include <dynamic_array.hpp>
#include <instruction_array.h>

namespace rs {
	class context;
	class execution_state;

	class runtime_exception : public std::exception {
		public:
			runtime_exception(const std::string& error, execution_state* state, const instruction_array::instruction& i);
			runtime_exception(const std::string& error, execution_state* state);
			runtime_exception(const std::string& error);
			~runtime_exception() { }

			virtual const char* what() { return text.c_str(); }
			std::string text;
			std::string file;
			std::string lineText;
			u32 line;
			u32 col;
			bool has_source_info;
	};

	class execution_state {
		public:
			execution_state(const context_parameters& params, context* ctx);
			~execution_state();

			void execute(integer_type entry_point, integer_type exit_point = rs_integer_max);

			void push_state();
			void pop_state(rs_register persist);
			void push_scope();
			void pop_scope();
			void print_instruction(integer_type idx, const instruction_array::instruction& i);
			inline context* ctx() { return m_ctx; }
			inline variable* registers() { return m_stack[m_stack_idx]; }
			inline integer_type instruction_addr() const { return m_current_instruction_idx; }

			struct scope {
				dynamic_pod_array<variable_id> allocated_vars;
			};

		protected:
			u32 m_last_printed_line;
			u32 m_last_printed_col;
			variable (*m_stack)[rs_register::register_count];
			size_t m_stack_idx;
			size_t m_stack_depth;
			context* m_ctx;
			integer_type m_current_instruction_idx;
	};
};