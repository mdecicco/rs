#pragma once
#include <defs.h>
#include <instruction_array.h>
#include <parse_utils.h>

#include <string>
#include <stack>

namespace rs {
	class variable;
	class script_compiler {
		public:
			script_compiler(const context_parameters& params);
			~script_compiler();

			struct var_ref {
				var_ref(context* ctx, const tokenizer::token& t, bool constant);
				var_ref(rs_register reg, const tokenizer::token& t, bool constant);
				var_ref(const variable& var);

				tokenizer::token name;
				bool is_const;
				variable_id id;
				rs_register reg;
			};
			typedef std::vector<var_ref> ref_vec;

			struct script_function {
				tokenizer::token name;
				ref_vec params;
				variable_id instruction_offset;
				u64 instruction_count;
				ref_vec declared_vars;
				ref_vec referenced_vars;
				std::vector<u32> reference_counts;
			};
			typedef std::vector<script_function*> func_vec;

			struct parse_context {
				std::string file;
				ref_vec globals;
				std::vector<ref_vec> locals;
				func_vec functions;
				u8 current_scope_idx;
				context* ctx;

				script_function* currentFunction;

				void push_locals();
				void pop_locals();
				var_ref var(const std::string& name);
				script_function* func(const std::string& name);
			};

			bool compile(const std::string& code, instruction_array& instructions);
			
		protected:
			context* m_script_context;
			script_function* compile_function(tokenizer& t, parse_context& ctx, instruction_array& instructions);
			bool compile_expression(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected);
			bool compile_statement(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool parseSemicolon = true);
			bool compile_identifier(const var_ref& variable, const tokenizer::token& reference, tokenizer& t, parse_context& ctx, instruction_array& instructions);
			bool compile_json(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected);
	};
};

