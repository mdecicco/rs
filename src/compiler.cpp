#include <compiler.h>
#include <context.h>
#include <parse_utils.h>
using namespace std;

namespace rs {
	using instruction = instruction_array::instruction;
	using token = tokenizer::token;

	script_compiler::script_compiler(const context_parameters& params) {
		m_script_context = params.ctx;
	}

	script_compiler::~script_compiler() {
	}

	script_compiler::var_ref::var_ref(context* ctx, const tokenizer::token& t, bool constant) {
		name = t;
		is_const = constant;
		id = t.valid() ? ctx->memory->gen_var_id() : 0;
		reg = rs_register::null_register;
	}

	script_compiler::var_ref::var_ref(rs_register _reg, const tokenizer::token& t, bool constant) {
		name = t;
		is_const = constant;
		id = 0;
		reg = _reg;
	}

	script_compiler::var_ref::var_ref(const variable& var) {
		name = var.name;
		is_const = var.is_const;
		id = var.id;
		reg = rs_register::null_register;
	}

	void script_compiler::parse_context::push_locals() {
		locals.push_back(ref_vec());
	}

	void script_compiler::parse_context::pop_locals() {
		locals.pop_back();
	}

	script_compiler::var_ref script_compiler::parse_context::var(const string& name) {
		for (auto& ref : globals) {
			if (ref.name.text == name) return ref;
		}

		for (auto i = locals.rbegin();i != locals.rend();i++) {
			ref_vec& vars = *i;
			for (auto& ref : vars) {
				if (ref.name.text == name) return ref;
			}
		}

		for (auto& i : ctx->global_variables) {
			if (i.name.text == name) return var_ref(i);
		}
		
		tokenizer::token t;
		return var_ref(nullptr, t, false);
	}

	script_compiler::script_function* script_compiler::parse_context::func(const string& name) {
		for (auto* func : functions) {
			if (func->name.text == name) return func;
		}

		for (auto* func : ctx->global_functions) {
			if (func->name.text == name) {
				auto* nf = new script_compiler::script_function;
				nf->instruction_offset = func->entry_point_id;
				nf->name = func->name;
				for (auto& p : func->params) {
					nf->params.push_back(var_ref(p));
				}
				return nf;
			}
		}
		
		return nullptr;
	}

	bool script_compiler::compile(const std::string& code, instruction_array& instructions) {
		tokenizer t("test", code);
		t.specify_keyword("function");
		t.specify_keyword("return");
		t.specify_keyword("this");
		t.specify_keyword("const");
		t.specify_keyword("let");
		t.specify_keyword("if");
		t.specify_keyword("while");
		t.specify_keyword("for");
		t.specify_keyword("else");
		t.specify_keyword("class");
		t.specify_keyword("extends");
		t.specify_keyword("export");
		t.specify_keyword("continue");
		t.specify_keyword("break");
		t.specify_keyword("?");
		t.specify_keyword(":");
		t.specify_keyword("+");
		t.specify_keyword("-");
		t.specify_keyword("*");
		t.specify_keyword("/");
		t.specify_keyword("%");
		t.specify_keyword("^");
		t.specify_keyword("=");
		t.specify_keyword("!");
		t.specify_keyword("<");
		t.specify_keyword(">");
		t.specify_keyword("++");
		t.specify_keyword("--");
		t.specify_keyword("+=");
		t.specify_keyword("-=");
		t.specify_keyword("*=");
		t.specify_keyword("/=");
		t.specify_keyword("%=");
		t.specify_keyword("^=");
		t.specify_keyword("==");
		t.specify_keyword("!=");
		t.specify_keyword("<=");
		t.specify_keyword(">=");
		t.specify_keyword("&&");
		t.specify_keyword("||");
		t.specify_keyword("=>");

		instructions.backup();
		try {
			parse_context ctx;
			ctx.ctx = m_script_context;
			ctx.currentFunction = nullptr;
			ctx.file = "test";
			ctx.current_scope_idx = 0;
			ctx.push_locals();

			while (!t.at_end()) {
				script_function* func = compile_function(t, ctx, instructions);
				if (func) {
					ctx.functions.push_back(func);
					auto f = new function(m_script_context, func->name, func->instruction_offset);
					for (auto& p : func->params) f->params.push_back({ p.id, p.name, p.is_const });
					for (auto& d : func->declared_vars) f->declared_vars.push(d.id);

					m_script_context->global_functions.push_back(f);
					continue;
				}

				compile_statement(t, ctx, instructions);
			}

			ctx.pop_locals();

			instructions.commit();
			return true;
		} catch (const parse_exception& e) {
			printf("%s:%d:%d: %s\n%s\n", e.file.c_str(), e.line + 1, e.col + 1, e.text.c_str(), e.lineText.c_str());
			for (i64 c = 0;c < i64(e.col);c++) printf(" ");
			printf("^\n");
			instructions.restore();
		}

		return false;
	}

	script_compiler::script_function* script_compiler::compile_function(tokenizer& t, parse_context& ctx, instruction_array& instructions) {
		auto func_kw = t.keyword(false, "function");
		if (!func_kw.valid()) return nullptr;

		ref_vec params;
		u64 first_instruction = instructions.count();

		ctx.push_locals();

		auto func_name = t.identifier();
		auto open_params = t.character('(');
		for(unsigned char i = 0;i < 8;i++) {
			auto pname = t.identifier(false);
			if (!pname.valid()) break;
			rs_register r = (rs_register)(rs_register::parameter0 + i);
			ctx.locals.back().push_back(var_ref(r, pname, true));
			params.push_back(var_ref(r, pname, true));

			auto comma = t.character(',', false);
			if (!comma.valid()) break;
		}
		auto close_params = t.character(')');
		auto body_open = t.character('{');

		script_function* func = new script_function;
		func->name = func_name;
		func->params = params;
		func->instruction_offset = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &first_instruction);
		ctx.currentFunction = func;

		bool closed = false;
		while(!t.at_end()) {
			auto body_close = t.character('}', false);
			if (body_close.valid()) {
				closed = true;
				break;
			}

			compile_statement(t, ctx, instructions);
		}

		ctx.currentFunction = nullptr;

		if (!closed) {
			throw parse_exception(
				"Encountered unexpected end of input while parsing function body",
				ctx.file,
				t.lines[body_open.line],
				body_open.line,
				body_open.col
			);
		}

		ctx.pop_locals();

		func->instruction_count = instructions.count() - first_instruction;

		return func;
	}

	bool script_compiler::compile_expression(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected) {
		if (!expected) {
			t.backup_state();
			instructions.backup();
		}

		bool is_const = true;
		var_ref var = ctx.var("");
		script_function* func = nullptr;
		token const_token;
		token identifier;

		auto popen = t.character('(', false);
		auto do_operator = [&t, &is_const, &var, &func, &ctx, &instructions, this](rs_instruction i, const string& op_kw, bool assign, bool hasRhs = true) {
			auto op = t.keyword(false, op_kw);
			if (op.valid()) {
				if (assign && is_const) {
					string msg = "";
					if (var.id) msg = format("'%s' was declared const and can not be reassigned", var.name.text.c_str());
					else if (func) msg = "Function call return values can not be assigned";
					else msg = "Expressions and constants can not be assigned";

					throw parse_exception(
						msg,
						ctx.file,
						t.lines[op.line],
						op.line,
						op.col
					);
				}

				if (!hasRhs) {
					instructions.append(
						instruction(i).arg(rs_register::lvalue),
						ctx.file,
						op.line,
						op.col,
						t.lines[op.line]
					);
					return true;
				}

				instructions.append(
					instruction(rs_instruction::pushState),
					ctx.file,
					op.line,
					op.col,
					t.lines[op.line]
				);

				// compile right side of expression, rhs expression result will be stored in rvalue
				this->compile_expression(t, ctx, instructions, true);

				instructions.append(
					instruction(rs_instruction::popState).arg(rs_register::rvalue),
					ctx.file,
					op.line,
					op.col,
					t.lines[op.line]
				);

				if (i != rs_instruction::null_instruction) {
					// Do the operation
					instructions.append(
						instruction(i).arg(rs_register::lvalue).arg(rs_register::rvalue),
						ctx.file,
						op.line,
						op.col,
						t.lines[op.line]
					);
				}

				return true;
			}

			return false;
		};

		if (!popen.valid()) {
			identifier = t.identifier(false);
			if (identifier.valid()) {
				var = ctx.var(identifier.text);
				func = ctx.func(identifier.text);

				if (!var.id && var.reg == rs_register::null_register && !func) {
					throw parse_exception(
						format("Use of undeclared identifier '%s'", identifier.text.c_str()),
						ctx.file,
						t.lines[identifier.line],
						identifier.line,
						identifier.col
					);
				}
			}
		}

		// populate lvalue
		if (var.id || var.reg != rs_register::null_register) {
			// variable
			is_const = compile_identifier(var, identifier, t, ctx, instructions);
			if (ctx.currentFunction) {
				bool found = false;
				for (u32 i = 0;i < ctx.currentFunction->referenced_vars.size();i++) {
					if (ctx.currentFunction->referenced_vars[i].name.text == var.name.text) {
						ctx.currentFunction->reference_counts[i]++;
						found = true;
						break;
					}
				}

				if (!found) {
					ctx.currentFunction->referenced_vars.push_back(var);
					ctx.currentFunction->reference_counts.push_back(1);
				}
			}
		}
		else if (func) {
			// function call
			token popen = t.character('(');

			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				popen.line,
				popen.col,
				t.lines[popen.line]
			);

			token pclose;
			token comma;
			for (u8 p = 0;p < 8;p++) {
				if (!pclose.valid()) pclose = t.character(')', false);
				if (!pclose.valid() && compile_expression(t, ctx, instructions, comma.valid() || !pclose.valid())) {
					comma = t.character(',', false);
					pclose = t.character(')', !comma.valid());
					instructions.append(
						instruction(rs_instruction::move).arg(rs_register(rs_register::parameter0 + p)).arg(rs_register::rvalue),
						ctx.file,
						(comma.valid() ? comma : pclose).line,
						(comma.valid() ? comma : pclose).col,
						t.lines[(comma.valid() ? comma : pclose).line]
					);
				} else {
					instructions.append(
						instruction(rs_instruction::move).arg(rs_register(rs_register::parameter0 + p)).arg(variable_id(0)),
						ctx.file,
						popen.line,
						popen.col,
						t.lines[popen.line]
					);
				}
			}

			instructions.append(
				instruction(rs_instruction::call).arg(func->instruction_offset),
				ctx.file,
				pclose.line,
				pclose.col,
				t.lines[pclose.line]
			);

			instructions.append(
				instruction(rs_instruction::popState),
				ctx.file,
				pclose.line,
				pclose.col,
				t.lines[pclose.line]
			);

			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::lvalue).arg(rs_register::return_value),
				ctx.file,
				pclose.line,
				pclose.col,
				t.lines[pclose.line]
			);
		}
		else if (popen.valid()) {
			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				popen.line,
				popen.col,
				t.lines[popen.line]
			);

			compile_expression(t, ctx, instructions, true);
			auto pclose = t.character(')');

			instructions.append(
				instruction(rs_instruction::popState).arg(rs_register::rvalue),
				ctx.file,
				pclose.line,
				pclose.col,
				t.lines[pclose.line]
			);

			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::lvalue).arg(rs_register::rvalue),
				ctx.file,
				pclose.line,
				pclose.col,
				t.lines[pclose.line]
			);
		}
		else {
			const_token = t.number_constant(false);
			if (const_token.valid()) {
				i32 num = atoll(const_token.text.c_str());
				variable_id vid = m_script_context->memory->set_static(
					rs_builtin_type::t_int32,
					sizeof(i32),
					(void*)(&num)
				);
				instructions.append(
					instruction(rs_instruction::move).arg(rs_register::lvalue).arg(vid),
					ctx.file,
					const_token.line,
					const_token.col,
					t.lines[const_token.line]
				);
			} else {
				const_token = t.string_constant(false);
				if (const_token.valid()) {
					variable_id vid = m_script_context->memory->set_static(
						rs_builtin_type::t_string,
						const_token.text.length(),
						(void*)const_token.text.c_str()
					);

					instructions.append(
						instruction(rs_instruction::move).arg(rs_register::lvalue).arg(vid),
						ctx.file,
						const_token.line,
						const_token.col,
						t.lines[const_token.line]
					);
				} else {
					if (!expected) {
						t.restore_state();
						instructions.restore();
					} else {
						throw parse_exception(
							"Expected expression",
							ctx.file,
							t.lines[t.line()],
							t.line(),
							t.col()
						);
					}

					return false;
				}
			}
		}

		if (!expected) {
			t.commit_state();
			instructions.commit();
		}

		bool compiled_rhs = false;
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::store    , "=", true);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::add      , "+"  , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::sub      , "-"  , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::mul      , "*"  , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::div      , "/"  , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::mod      , "%"  , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::pow      , "^"  , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::or       , "||" , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::and      , "&&" , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::less     , "<"  , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::greater  , ">"  , false);

		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::addEq    , "+=" , true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::subEq    , "-=" , true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::mulEq    , "*=" , true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::divEq    , "/=" , true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::modEq    , "%=" , true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::powEq    , "^=" , true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::orEq     , "||=", true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::andEq    , "&&=", true );
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::lessEq   , "<=" , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::greaterEq, ">=" , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::inc      , "++" , true , false);
		if (!compiled_rhs) compiled_rhs = do_operator(rs_instruction::dec      , "--" , true , false);

		if (compiled_rhs) return true;

		instructions.append(
			instruction(rs_instruction::move).arg(rs_register::rvalue).arg(rs_register::lvalue),
			ctx.file,
			t.line(),
			t.col(),
			t.lines[t.line()]
		);

		return true;
	}

	bool script_compiler::compile_statement(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool parseSemicolon) {
		// Statements can start with:
		//	- return, if, while, for, const, let
		//	- expressions (but the result will be ignored by the rest of the code)

		t.backup_state();

		auto kw = t.keyword(false);
		if (kw.valid()) {
			if (kw.text == "return") {
				compile_expression(t, ctx, instructions, true);
				instructions.append(
					instruction(rs_instruction::move).arg(rs_register::return_value).arg(rs_register::rvalue),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);
				instructions.append(
					instruction(rs_instruction::ret),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);
				if (parseSemicolon) t.character(';');
			}
			else if (kw.text == "if") {
				ctx.current_scope_idx++;
				t.character('(');
				compile_expression(t, ctx, instructions, true);
				t.character(')');

				u64 pass_address = instructions.count() + 1;
				variable_id pass_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &pass_address);

				instruction& branch = instructions.append(
					instruction(rs_instruction::branch).arg(rs_register::rvalue).arg(pass_addr_id),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);

				instructions.append(
					instruction(rs_instruction::pushScope),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);

				auto block_open = t.character('{', false);
				if (block_open.valid()) {
					ctx.push_locals();
					token closed;
					while (!t.at_end()) {
						closed = t.character('}', false);
						if (closed.valid()) break;
						compile_statement(t, ctx, instructions);
					}
					if (!closed.valid()) {
						throw parse_exception(
							"Encountered unexpected end of input while parsing 'if' statement body",
							ctx.file,
							t.lines[block_open.line],
							block_open.line,
							block_open.col
						);
					}

					instructions.append(
						instruction(rs_instruction::popScope),
						ctx.file,
						closed.line,
						closed.col,
						t.lines[closed.line]
					);

					u64 fail_address = instructions.count();
					variable_id fail_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &fail_address);
					branch.arg(fail_addr_id);
					ctx.pop_locals();
				} else {
					compile_statement(t, ctx, instructions, false);
					auto sc = t.semicolon();

					instructions.append(
						instruction(rs_instruction::popScope),
						ctx.file,
						sc.line,
						sc.col,
						t.lines[sc.line]
					);

					u64 fail_address = instructions.count();
					variable_id fail_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &fail_address);
					branch.arg(fail_addr_id);
				}

				ctx.current_scope_idx--;
			}
			else if (kw.text == "while") {
				ctx.current_scope_idx++;
				u64 expr_address = instructions.count() + 1;
				variable_id expr_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &expr_address);

				t.character('(');
				compile_expression(t, ctx, instructions, true);
				t.character(')');

				u64 pass_address = instructions.count() + 1;
				variable_id pass_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &pass_address);

				instruction& branch = instructions.append(
					instruction(rs_instruction::branch).arg(rs_register::rvalue).arg(pass_addr_id),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);

				auto block_open = t.character('{', false);
				if (block_open.valid()) {
					ctx.push_locals();
					instructions.append(
						instruction(rs_instruction::pushScope),
						ctx.file,
						block_open.line,
						block_open.col,
						t.lines[block_open.line]
					);

					token closed;
					while (!t.at_end()) {
						closed = t.character('}', false);
						if (closed.valid()) break;

						compile_statement(t, ctx, instructions);
					}
					if (!closed.valid()) {
						throw parse_exception(
							"Encountered unexpected end of input while parsing 'for' loop body",
							ctx.file,
							t.lines[block_open.line],
							block_open.line,
							block_open.col
						);
					}

					instructions.append(
						instruction(rs_instruction::popScope),
						ctx.file,
						closed.line,
						closed.col,
						t.lines[closed.line]
					);
					ctx.pop_locals();
				} else {
					compile_statement(t, ctx, instructions, parseSemicolon);
				}

				instructions.append(
					instruction(rs_instruction::store).arg(rs_register::instruction_address).arg(expr_addr_id),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);

				u64 fail_address = instructions.count();
				variable_id fail_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &fail_address);
				branch.arg(fail_addr_id);

				ctx.current_scope_idx--;
			}
			else if (kw.text == "for") {
				ctx.current_scope_idx++;
				auto open = t.character('(');
				ctx.push_locals();
				instructions.append(
					instruction(rs_instruction::pushScope),
					ctx.file,
					open.line,
					open.col,
					t.lines[open.line]
				);

				auto sc = t.semicolon(false);
				if (!sc.valid()) {
					compile_statement(t, ctx, instructions);
				}
				u64 expr_address = instructions.count();
				variable_id expr_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &expr_address);

				sc = t.semicolon(false);
				if (!sc.valid()) {
					compile_expression(t, ctx, instructions, true);
					t.semicolon();
				}

				u64 pass_address = instructions.count() + 1;
				variable_id pass_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &pass_address);

				instruction& branch = instructions.append(
					instruction(rs_instruction::branch).arg(rs_register::rvalue).arg(pass_addr_id),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);

				auto end_paren = t.character(')', false);
				if (!end_paren.valid()) {
					compile_statement(t, ctx, instructions, false);
					t.character(')');
				}

				auto block_open = t.character('{', false);
				if (block_open.valid()) {
					token closed;
					while (!t.at_end()) {
						closed = t.character('}', false);
						if (closed.valid()) break;

						compile_statement(t, ctx, instructions);
					}
					if (!closed.valid()) {
						throw parse_exception(
							"Encountered unexpected end of input while parsing 'for' loop body",
							ctx.file,
							t.lines[block_open.line],
							block_open.line,
							block_open.col
						);
					}

					instructions.append(
						instruction(rs_instruction::store).arg(rs_register::instruction_address).arg(expr_addr_id),
						ctx.file,
						closed.line,
						closed.col,
						t.lines[closed.line]
					);

					u64 fail_address = instructions.count();
					variable_id fail_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &fail_address);
					branch.arg(fail_addr_id);

					instructions.append(
						instruction(rs_instruction::popScope),
						ctx.file,
						closed.line,
						closed.col,
						t.lines[closed.line]
					);
				} else {
					compile_statement(t, ctx, instructions, false);
					auto sc = parseSemicolon ? t.character(';') : token();

					instructions.append(
						instruction(rs_instruction::store).arg(rs_register::instruction_address).arg(expr_addr_id),
						ctx.file,
						sc.valid() ? sc.line : t.line(),
						sc.valid() ? sc.col : t.col(),
						t.lines[sc.valid() ? sc.line : t.line()]
					);

					u64 fail_address = instructions.count();
					variable_id fail_addr_id = m_script_context->memory->set_static(rs_builtin_type::t_uint64, sizeof(u64), &fail_address);
					branch.arg(fail_addr_id);

					instructions.append(
						instruction(rs_instruction::popScope),
						ctx.file,
						sc.valid() ? sc.line : t.line(),
						sc.valid() ? sc.col : t.col(),
						t.lines[sc.valid() ? sc.line : t.line()]
					);
				}
				ctx.current_scope_idx--;
			}
			else if (kw.text == "const") {
				t.backup_state();
				auto var_name = t.identifier();
				bool uninitialized = t.semicolon(false).valid();
				// expression parser needs to see this too
				t.restore_state();
				auto existing = ctx.var(var_name.text);
				if (existing.id) {
					throw parse_exception(
						format("Cannot redeclare '%s', previous definition is on %s:%d", var_name.text.c_str(), existing.name.file.c_str(), existing.name.line + 1),
						ctx.file,
						t.lines[var_name.line],
						var_name.line,
						var_name.col
					);
				}

				script_function* func = ctx.func(var_name.text);
				if (func) {
					throw parse_exception(
						format("Cannot redeclare '%s', previous definition is on %s:%d", var_name.text.c_str(), func->name.file.c_str(), func->name.line + 1),
						ctx.file,
						t.lines[var_name.line],
						var_name.line,
						var_name.col
					);
				}
				
				// Variable should be non-const initially to allow
				// the initializer expression to assign a value to it
				var_ref ref = var_ref(m_script_context, var_name, false);
				ctx.locals.back().push_back(ref);
				if (!uninitialized) compile_expression(t, ctx, instructions, true);
				ctx.locals.back().back().is_const = true;
				ref.is_const = true;

				if (ctx.currentFunction) ctx.currentFunction->declared_vars.push_back(ref);
				else if (ctx.current_scope_idx == 0) m_script_context->global_variables.push_back({ ref.id, ref.name, ref.is_const });

				if (parseSemicolon) t.character(';');
			}
			else if (kw.text == "let") {
				t.backup_state();
				auto var_name = t.identifier();
				bool uninitialized = t.semicolon(false).valid();
				// expression parser needs to see this too
				t.restore_state();
				auto existing = ctx.var(var_name.text);
				if (existing.id || existing.reg != rs_register::null_register) {
					throw parse_exception(
						format("Cannot redeclare '%s', previous definition is on %s:%d", var_name.text.c_str(), existing.name.file.c_str(), existing.name.line + 1),
						ctx.file,
						t.lines[var_name.line],
						var_name.line,
						var_name.col
					);
				}

				script_function* func = ctx.func(var_name.text);
				if (func) {
					throw parse_exception(
						format("Cannot redeclare '%s', previous definition is on %s:%d", var_name.text.c_str(), func->name.file.c_str(), func->name.line + 1),
						ctx.file,
						t.lines[var_name.line],
						var_name.line,
						var_name.col
					);
				}
				auto ref = var_ref(m_script_context, var_name, false);
				ctx.locals.back().push_back(ref);
				if (ctx.currentFunction) ctx.currentFunction->declared_vars.push_back(ref);
				else if (ctx.current_scope_idx == 0) m_script_context->global_variables.push_back({ ref.id, ref.name, ref.is_const });

				if (!uninitialized) compile_expression(t, ctx, instructions, true);
				if (parseSemicolon) t.character(';');
			}
			else return false;

			return true;
		}

		t.restore_state();
		bool compiled = compile_expression(t, ctx, instructions, true);
		if (parseSemicolon) t.character(';');

		return compiled;
	}

	bool script_compiler::compile_identifier(const var_ref& variable, const tokenizer::token& reference, tokenizer& t, parse_context& ctx, instruction_array& instructions) {
		if (reference.valid()) {
			auto i = instruction(rs_instruction::move).arg(rs_register::lvalue);
			if (variable.id) i.arg(variable.id);
			else i.arg(variable.reg);
			instructions.append(
				i,
				ctx.file,
				reference.line,
				reference.col,
				t.lines[reference.line]
			);
		}
		
		auto accessor_operator = t.character('.', false);
		if (accessor_operator.valid()) {
			auto prop_name = t.identifier();
			variable_id vid = m_script_context->memory->set_static(
				rs_builtin_type::t_string,
				prop_name.text.length(),
				(void*)prop_name.text.c_str()
			);
			auto i = instruction(rs_instruction::prop).arg(rs_register::lvalue).arg(vid);
			instructions.append(
				i,
				ctx.file,
				prop_name.line,
				prop_name.col,
				t.lines[prop_name.line]
			);

			compile_identifier(var_ref(m_script_context, tokenizer::token(), false), tokenizer::token(), t, ctx, instructions);
			// lvalue will now be pointing to the accessed variable at the
			// end of the accessor path

			return false;
		}

		auto index_open_operator = t.character('[', false);
		if (index_open_operator.valid()) {
			auto num = t.number_constant(false);
			if (num.valid()) {
				u32 idx = atoll(num.text.c_str());
				variable_id vid = m_script_context->memory->set_static(
					rs_builtin_type::t_uint32,
					sizeof(u32),
					(void*)(&idx)
				);
				auto i = instruction(rs_instruction::prop).arg(rs_register::lvalue).arg(vid);
				instructions.append(
					i,
					ctx.file,
					num.line,
					num.col,
					t.lines[num.line]
				);
				t.character(']');
			} else {
				auto str = t.string_constant(false);
				if (str.valid()) {
					variable_id vid = m_script_context->memory->set_static(
						rs_builtin_type::t_string,
						str.text.length(),
						(void*)str.text.c_str()
					);

					instructions.append(
						instruction(rs_instruction::prop).arg(rs_register::lvalue).arg(vid),
						ctx.file,
						str.line,
						str.col,
						t.lines[str.line]
					);
					t.character(']');
				} else {
					instructions.append(
						instruction(rs_instruction::pushState),
						ctx.file,
						index_open_operator.line,
						index_open_operator.col,
						t.lines[index_open_operator.line]
					);

					compile_expression(t, ctx, instructions, true);

					t.character(']');

					instructions.append(
						instruction(rs_instruction::move).arg(rs_register::return_value).arg(rs_register::rvalue),
						ctx.file,
						index_open_operator.line,
						index_open_operator.col,
						t.lines[index_open_operator.line]
					);

					instructions.append(
						instruction(rs_instruction::popState),
						ctx.file,
						index_open_operator.line,
						index_open_operator.col,
						t.lines[index_open_operator.line]
					);

					instructions.append(
						instruction(rs_instruction::prop).arg(rs_register::lvalue).arg(rs_register::return_value),
						ctx.file,
						index_open_operator.line,
						index_open_operator.col,
						t.lines[index_open_operator.line]
					);
				}
			}

			return false;
		}

		return variable.is_const;
	}
};