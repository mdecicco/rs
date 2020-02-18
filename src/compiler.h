#pragma once
#include <defs.h>
#include <instruction_array.h>
#include <parse_utils.h>

#include <string>
#include <stack>

namespace rs {
	class variable;
	class object_prototype;
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

			struct function_ref {
				tokenizer::token name;
				ref_vec params;
				variable_id function_id;
				variable_id instruction_offset;
				integer_type instruction_count;
				ref_vec declared_vars;
				ref_vec referenced_vars;
				std::vector<u32> reference_counts;
				bool is_global;
				bool has_explicit_return;
			};
			typedef std::vector<function_ref*> func_vec;

			struct parse_context {
				std::string file;
				ref_vec globals;
				std::vector<ref_vec> locals;
				std::vector<object_prototype*> prototypes;
				std::vector<var_ref> prototypeStaticVars;
				func_vec functions;
				u8 current_scope_idx;
				context* ctx;

				function_ref* currentFunction;
				object_prototype* currentPrototype;
				bool constructingPrototype;

				void push_locals();
				void pop_locals();
				var_ref var(const std::string& name);
				variable_id func(const std::string& name);
				object_prototype* proto(const std::string& name);
			};

			bool compile(const std::string& code, instruction_array& instructions);
			
		protected:
			context* m_script_context;
			void initialize_tokenizer(tokenizer& t);
			void check_declaration(tokenizer& t, parse_context& ctx, tokenizer::token& declaration);
			function_ref* compile_function(tokenizer& t, parse_context& ctx, instruction_array& instructions, rs_register destination = rs_register::lvalue, bool allow_name = true);
			object_prototype* compile_class(tokenizer& t, parse_context& ctx, instruction_array& instructions);
			bool compile_expression(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected, rs_register destination = rs_register::rvalue, bool compile_lhs = true);
			bool compile_expression_value(rs_register destination, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected, bool& value_is_const);
			bool compile_accessor_chain(rs_register destination, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool is_nested);
			bool compile_statement(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool parseSemicolon = true);
			bool compile_identifier(const var_ref& variable, const tokenizer::token& reference, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool& pushed_state);
			bool compile_parameter_list(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected);
			bool compile_json(rs_register destination, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected);
	};
};

