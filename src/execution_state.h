#pragma once
#include <defs.h>
#include <dynamic_array.hpp>
#include <instruction_array.h>

namespace rs {
	class context;

	class execution_state {
		public:
			execution_state(const context_parameters& params, context* ctx);
			~execution_state();

			void execute_all(integer_type entry_point);
			void execute_next();

			void push_state();
			void pop_state(rs_register persist);
			void push_scope();
			void pop_scope();
			void print_instruction(integer_type idx, const instruction_array::instruction& i);
			inline context* ctx() { return m_ctx; }
			inline register_type* registers() { return m_stack[m_stack_idx]; }

			struct scope {
				dynamic_pod_array<variable_id> allocated_vars;
			};

		protected:
			u32 m_last_printed_line;
			u32 m_last_printed_col;
			register_type (*m_stack)[rs_register::register_count];
			size_t m_stack_idx;
			size_t m_stack_depth;
			context* m_ctx;
	};
};