#include <compiler.h>
#include <context.h>
#include <parse_utils.h>
#include <script_object.h>
#include <script_function.h>
#include <prototype.h>
using namespace std;

namespace rs {
	using instruction = instruction_array::instruction;
	using token = tokenizer::token;

	variable_id define_static_number(context* ctx, integer_type val) {
		variable_id vid = ctx->memory->find_static([&val](const variable& var) {
			return var.type() == rs_builtin_type::t_integer && integer_type(var) == val;
		});
		if (vid) return vid;
		vid = ctx->memory->create(rs_variable_flags::f_const | rs_variable_flags::f_static);
		ctx->memory->get(vid).set(val);
		return vid;
	}

	variable_id define_static_number(context* ctx, const token& tok, const script_compiler::parse_context& pctx, const tokenizer& t) {
		variable_id vid = 0;

		if (tok.text.find_first_of(".") != string::npos) {
			double num = atof(tok.text.c_str());
			decimal_type v = num;
			if (num < rs_decimal_min || num > rs_decimal_max) {
				throw parse_exception(
					format("Number '%s' is too large for internal data type", tok.text.c_str()),
					pctx.file,
					t.lines[tok.line],
					tok.line,
					tok.col
				);
			}
			vid = ctx->memory->find_static([&v](const variable& var) {
				return var.type() == rs_builtin_type::t_decimal && decimal_type(var) == v;
			});
			if (vid) return vid;
			vid = ctx->memory->create(rs_variable_flags::f_const | rs_variable_flags::f_static);
			ctx->memory->get(vid).set(v);
		} else {
			auto num = atoll(tok.text.c_str());
			integer_type v = num;
			if (num < rs_integer_min || num > rs_integer_max) {
				throw parse_exception(
					format("Number '%s' is too large for internal data type", tok.text.c_str()),
					pctx.file,
					t.lines[tok.line],
					tok.line,
					tok.col
				);
			}

			vid = ctx->memory->find_static([&v](const variable& var) {
				return var.type() == rs_builtin_type::t_integer && integer_type(var) == v;
			});
			if (vid) return vid;
			vid = ctx->memory->create(rs_variable_flags::f_const | rs_variable_flags::f_static);
			ctx->memory->get(vid).set(v);
		}

		return vid;
	}

	variable_id define_static_string(context* ctx, const string& t) {
		variable_id existing = ctx->memory->find_static([&t](const variable& var) {
			return var.type() == rs_builtin_type::t_string && var.to_string() == t;
		});
		if (existing) return existing;

		char* str = new char[t.length()];
		memcpy(str, t.c_str(), t.length());
		variable_id vid = ctx->memory->create(rs_variable_flags::f_const | rs_variable_flags::f_static);
		ctx->memory->get(vid).set(str, t.length());
		return vid;
	}



	script_compiler::script_compiler(const context_parameters& params) {
		m_script_context = params.ctx;
	}

	script_compiler::~script_compiler() {
	}



	script_compiler::var_ref::var_ref(context* ctx, const tokenizer::token& t, bool constant) {
		name = t;
		is_const = constant;
		id = t.valid() ? ctx->memory->create(rs_variable_flags::f_const) : 0;
		reg = rs_register::null_register;
	}

	script_compiler::var_ref::var_ref(rs_register _reg, const tokenizer::token& t, bool constant) {
		name = t;
		is_const = constant;
		id = 0;
		reg = _reg;
	}

	script_compiler::var_ref::var_ref(const variable_reference& var) {
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
		if (name == "this") {
			tokenizer::token t = { 0, 0, "this", "internal" };
			return var_ref(rs_register::this_obj, t, true);
		}

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

	variable_id script_compiler::parse_context::func(const string& name) {
		for (auto* func : functions) {
			if (func->name.text == name) return func->function_id;
		}

		for (auto* func : ctx->global_functions) {
			if (func->name.text == name) {
				return func->function_id;
			}
		}
		
		return 0;
	}

	object_prototype* script_compiler::parse_context::proto(const string& name) {
		for (auto* proto : prototypes) {
			if (proto->name() == name) return proto;
		}

		for (auto* proto : ctx->prototypes) {
			if (proto->name() == name) return proto;
		}

		return nullptr;
	}



	bool script_compiler::compile(const std::string& code, instruction_array& instructions) {
		tokenizer t("test", code);
		initialize_tokenizer(t);

		instructions.backup();
		try {
			parse_context ctx;
			ctx.ctx = m_script_context;
			ctx.currentFunction = nullptr;
			ctx.currentPrototype = nullptr;
			ctx.constructingPrototype = false;
			ctx.file = "test";
			ctx.current_scope_idx = 0;
			ctx.push_locals();

			while (!t.at_end()) {
				compile_statement(t, ctx, instructions);
			}

			for (function_ref* func : ctx.functions) {
				auto f = new script_function(m_script_context, func->name, func->instruction_offset, func->instruction_count);
				f->function_id = func->function_id;
				for (auto& p : func->params) f->params.push_back({ p.id, p.name, p.is_const });
				for (auto& d : func->declared_vars) f->declared_vars.push(d.id);
				m_script_context->memory->get(func->function_id).set(f);
				if (func->is_global) m_script_context->global_functions.push_back(f);
			}

			for (object_prototype* proto : ctx.prototypes) {
				m_script_context->prototypes.push_back(proto);
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

	void script_compiler::initialize_tokenizer(tokenizer& t) {
		t.specify_keyword("function");
		t.specify_keyword("return");
		t.specify_keyword("const");
		t.specify_keyword("let");
		t.specify_keyword("if");
		t.specify_keyword("else");
		t.specify_keyword("while");
		t.specify_keyword("for");
		t.specify_keyword("else");
		t.specify_keyword("class");
		t.specify_keyword("extends");
		t.specify_keyword("export");
		t.specify_keyword("continue");
		t.specify_keyword("break");
		t.specify_keyword("new");
		t.specify_keyword("static");
		t.specify_keyword("constructor");
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
	}

	void script_compiler::check_declaration(tokenizer& t, parse_context& ctx, token& declaration) {
		if (ctx.currentPrototype) {
			script_function* method = ctx.currentPrototype->method(declaration.text);
			if (method) {
				throw parse_exception(
					format("Cannot redeclare class method '%s', previous definition is on %s:%d", declaration.text.c_str(), method->name.file.c_str(), method->name.line + 1),
					ctx.file,
					t.lines[declaration.line],
					declaration.line,
					declaration.col
				);
			}

			method = ctx.currentPrototype->static_method(declaration.text);
			if (method) {
				throw parse_exception(
					format("Cannot redeclare static class method '%s', previous definition is on %s:%d", declaration.text.c_str(), method->name.file.c_str(), method->name.line + 1),
					ctx.file,
					t.lines[declaration.line],
					declaration.line,
					declaration.col
				);
			}

			variable_id staticVar = ctx.currentPrototype->static_variable(declaration.text);
			if (staticVar) {
				for (auto& v : ctx.prototypeStaticVars) {
					if (v.id == staticVar) {
						throw parse_exception(
							format("Cannot redeclare static class variable '%s', previous definition is on %s:%d", declaration.text.c_str(), v.name.file.c_str(), v.name.line + 1),
							ctx.file,
							t.lines[declaration.line],
							declaration.line,
							declaration.col
						);
					}
				}

				throw parse_exception(
					format("Cannot redeclare static class variable '%s'", declaration.text.c_str()),
					ctx.file,
					t.lines[declaration.line],
					declaration.line,
					declaration.col
				);
			}

			return;
		}

		auto existing = ctx.var(declaration.text);
		if (existing.id || existing.reg != rs_register::null_register) {
			throw parse_exception(
				format("Cannot redeclare '%s', previous definition is on %s:%d", declaration.text.c_str(), existing.name.file.c_str(), existing.name.line + 1),
				ctx.file,
				t.lines[declaration.line],
				declaration.line,
				declaration.col
			);
		}

		variable_id func = ctx.func(declaration.text);
		if (func) {
			token func_name;
			for (auto* func : ctx.functions) {
				if (func->name.text == declaration.text) {
					func_name = func->name;
					break;
				}
			}

			for (auto* func : m_script_context->global_functions) {
				if (func->name.text == declaration.text) {
					func_name = func->name;
					break;
				}
			}

			throw parse_exception(
				format("Cannot redeclare '%s', previous definition is on %s:%d", declaration.text.c_str(), func_name.file.c_str(), func_name.line + 1),
				ctx.file,
				t.lines[declaration.line],
				declaration.line,
				declaration.col
			);
		}

		object_prototype* proto = ctx.proto(declaration.text);
		if (proto) {
			throw parse_exception(
				format("Cannot redeclare '%s', previous definition is on %s:%d", declaration.text.c_str(), proto->declaration().file.c_str(), proto->declaration().line + 1),
				ctx.file,
				t.lines[declaration.line],
				declaration.line,
				declaration.col
			);
		}
	}

	script_compiler::function_ref* script_compiler::compile_function(tokenizer& t, parse_context& ctx, instruction_array& instructions, rs_register destination, bool allow_name) {
		instruction& jump_over = instructions.append(
			instruction(rs_instruction::jump),
			ctx.file,
			t.line(),
			t.col(),
			t.lines[t.line()]
		);

		ref_vec params;
		integer_type first_instruction = instructions.count();

		ctx.push_locals();

		auto func_name = t.identifier(false);
		if (func_name.valid() && !allow_name) {
			throw parse_exception(
				"Named function can not be declared here",
				ctx.file,
				t.lines[func_name.line],
				func_name.line,
				func_name.col
			);
		}
		check_declaration(t, ctx, func_name);

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

		function_ref* func = new function_ref;
		func->function_id = m_script_context->memory->create(rs_variable_flags::f_const | rs_variable_flags::f_static);
		func->name = func_name;
		func->params = params;
		func->instruction_offset = define_static_number(m_script_context, first_instruction);
		func->is_global = !ctx.currentFunction && !ctx.currentPrototype;
		func->has_explicit_return = false;
		m_script_context->memory->get(func->function_id).set(func);
		ctx.currentFunction = func;
		if (!ctx.currentPrototype) ctx.functions.push_back(func);

		bool closed = false;
		token body_close;
		while(!t.at_end()) {
			body_close = t.character('}', false);
			if (body_close.valid()) {
				closed = true;
				break;
			}

			compile_statement(t, ctx, instructions);
		}

		if (!closed) {
			throw parse_exception(
				"Encountered unexpected end of input while parsing function body",
				ctx.file,
				t.lines[body_open.line],
				body_open.line,
				body_open.col
			);
		}

		if (!func->has_explicit_return) {
			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::return_value).arg(variable_id(0)),
				ctx.file,
				body_close.line,
				body_close.col,
				t.lines[body_close.line]
			);

			instructions.append(
				instruction(rs_instruction::ret),
				ctx.file,
				body_close.line,
				body_close.col,
				t.lines[body_close.line]
			);
		}

		ctx.currentFunction = nullptr;

		ctx.pop_locals();

		func->instruction_count = instructions.count() - first_instruction;
		jump_over.arg(define_static_number(m_script_context, instructions.count()));

		instructions.append(
			instruction(rs_instruction::move).arg(destination).arg(func->function_id),
			ctx.file,
			body_close.line,
			body_close.col,
			t.lines[body_close.line]
		);

		return func;
	}

	object_prototype* script_compiler::compile_class(tokenizer& t, parse_context& ctx, instruction_array& instructions) {
		token class_name = t.identifier();
		token inheritance_token = t.character(':', false);
		token inherits_class;
		if (inheritance_token.valid()) inherits_class = t.identifier();

		object_prototype* proto = new object_prototype(m_script_context, class_name);
		ctx.currentPrototype = proto;

		token open = t.character('{');
		token close = t.character('}', false);
		while (!t.at_end() && !close.valid()) {
			token static_kw = t.keyword(false, "static");
			token ctor_kw = t.keyword(false, "constructor");
			if (static_kw.valid()) {
				auto var_name = t.identifier();

				check_declaration(t, ctx, var_name);

				auto initialized = t.keyword(false, "=");
				
				var_ref ref = var_ref(m_script_context, var_name, false);

				if (initialized.valid()) {
					compile_expression(t, ctx, instructions, true);
					instructions.append(
						instruction(rs_instruction::store).arg(ref.id).arg(rs_register::rvalue),
						ctx.file,
						initialized.line,
						initialized.col,
						t.lines[initialized.line]
					);
					t.character(';');

					proto->static_variable(var_name.text, ref.id);
					ctx.prototypeStaticVars.push_back(ref);
				} else if (t.semicolon(false).valid()) {
					ctx.prototypeStaticVars.push_back(ref);
					proto->static_variable(var_name.text, ref.id);
				} else {
					// must be a static function
					function_ref* func = compile_function(t, ctx, instructions);
					auto f = new script_function(m_script_context, var_name, func->instruction_offset, func->instruction_count);
					f->function_id = func->function_id;
					for (auto& p : func->params) f->params.push_back({ p.id, p.name, p.is_const });
					for (auto& d : func->declared_vars) f->declared_vars.push(d.id);
					m_script_context->memory->get(func->function_id).set(f);
					proto->static_method(f);
				}
			} else if (ctor_kw.valid()) {
				function_ref* func = compile_function(t, ctx, instructions);
				auto f = new script_function(m_script_context, ctor_kw, func->instruction_offset, func->instruction_count);
				f->function_id = func->function_id;
				for (auto& p : func->params) f->params.push_back({ p.id, p.name, p.is_const });
				for (auto& d : func->declared_vars) f->declared_vars.push(d.id);
				m_script_context->memory->get(func->function_id).set(f);
				proto->constructor(f);
			} else {
				function_ref* func = compile_function(t, ctx, instructions);
				auto f = new script_function(m_script_context, func->name, func->instruction_offset, func->instruction_count);
				f->function_id = func->function_id;
				for (auto& p : func->params) f->params.push_back({ p.id, p.name, p.is_const });
				for (auto& d : func->declared_vars) f->declared_vars.push(d.id);
				m_script_context->memory->get(func->function_id).set(f);
				proto->method(f);
			}

			close = t.character('}', false);
		}

		if (!close.valid()) {
			throw parse_exception(
				"Encountered unexpected end of input while parsing class body",
				ctx.file,
				t.lines[open.line],
				open.line,
				open.col
			);
		}

		t.semicolon();

		ctx.currentPrototype = nullptr;
		ctx.prototypeStaticVars.clear();

		return proto;
	}

	bool script_compiler::compile_expression(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected, rs_register destination, bool compile_lhs) {
		if (!expected) {
			t.backup_state();
			instructions.backup();
		}

		bool lhs_is_const = false;
		if (compile_lhs) {
			bool compiled_lhs = compile_expression_value(rs_register::lvalue, t, ctx, instructions, expected, lhs_is_const);
			if (!compiled_lhs) {
				if (expected) {
					throw parse_exception(
						"Expected expression",
						ctx.file,
						t.lines[t.line()],
						t.line(),
						t.col()
					);
				} else {
					t.restore_state();
					instructions.restore();
				}

				return false;
			}
		} else lhs_is_const = true;

		auto do_operator = [&t, &ctx, &instructions, &lhs_is_const, &destination, this](rs_instruction i, const string& op_kw, bool assign, rs_register result_register = rs_register::rvalue, bool hasRhs = true) {
			token op = t.keyword(false, op_kw);
			if (!op.valid()) return false;
			
			if (assign && lhs_is_const) {
				throw parse_exception(
					"Left side of expression can not be assigned",
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

				if (result_register != destination) {
					instructions.append(
						instruction(rs_instruction::move).arg(destination).arg(result_register),
						ctx.file,
						op.line,
						op.col,
						t.lines[op.line]
					);
				}
				return true;
			}

			bool rhs_is_const = false;
			this->compile_expression_value(rs_register::rvalue, t, ctx, instructions, true, rhs_is_const);

			instructions.append(
				instruction(i).arg(rs_register::lvalue).arg(rs_register::rvalue),
				ctx.file,
				op.line,
				op.col,
				t.lines[op.line]
			);

			if (result_register != destination) {
				instructions.append(
					instruction(rs_instruction::move).arg(destination).arg(result_register),
					ctx.file,
					op.line,
					op.col,
					t.lines[op.line]
				);
			}

			return true;
		};
		
		bool compiled_op = false;
		if (!compiled_op) compiled_op = do_operator(rs_instruction::compare  , "==" , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::addEq    , "+=" , true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::subEq    , "-=" , true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::mulEq    , "*=" , true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::divEq    , "/=" , true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::modEq    , "%=" , true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::powEq    , "^=" , true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::orEq     , "||=", true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::andEq    , "&&=", true );
		if (!compiled_op) compiled_op = do_operator(rs_instruction::lessEq   , "<=" , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::greaterEq, ">=" , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::inc      , "++" , true , rs_register::rvalue, false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::dec      , "--" , true , rs_register::rvalue, false);

		if (!compiled_op) compiled_op = do_operator(rs_instruction::store    , "=", true);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::add      , "+"  , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::sub      , "-"  , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::mul      , "*"  , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::div      , "/"  , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::mod      , "%"  , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::pow      , "^"  , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::or       , "||" , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::and      , "&&" , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::less     , "<"  , false);
		if (!compiled_op) compiled_op = do_operator(rs_instruction::greater  , ">"  , false);

		if (compiled_op) {
			// maybe more expression to compile
			instructions.backup();

			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::lvalue).arg(destination),
				ctx.file,
				t.line(),
				t.col(),
				t.lines[t.line()]
			);

			bool compiled_more = compile_expression(t, ctx, instructions, false, destination, false);

			if (compiled_more) instructions.commit();
			else instructions.restore();

			if (!expected) {
				t.commit_state();
				instructions.commit();
			}
			return true;
		} else if (!compile_lhs) return false;

		if (rs_register::lvalue != destination) {
			instructions.append(
				instruction(rs_instruction::move).arg(destination).arg(rs_register::lvalue),
				ctx.file,
				t.line(),
				t.col(),
				t.lines[t.line()]
			);
		}

		if (!expected) {
			t.commit_state();
			instructions.commit();
		}
		return true;

		/*
		object_prototype* proto = nullptr;
		if (proto) {
			bool is_instantiation = compile_parameter_list(t, ctx, instructions, new_kw.valid());
			if (is_instantiation && !new_kw.valid()) {
				throw parse_exception(
					format("Must use 'new' to instantiate object of class '%s'", proto->name().c_str()),
					ctx.file,
					t.lines[identifier.line],
					identifier.line,
					identifier.col
				);
			}

			if (is_instantiation) {
				if (proto->constructor()) {
					instructions.append(
						instruction(rs_instruction::newObj),
						ctx.file,
						new_kw.line,
						new_kw.col,
						t.lines[new_kw.line]
					);

					instructions.append(
						instruction(rs_instruction::addProto).arg(rs_register::lvalue).arg(proto->id()),
						ctx.file,
						new_kw.line,
						new_kw.col,
						t.lines[new_kw.line]
					);

					instructions.append(
						instruction(rs_instruction::move).arg(rs_register::this_obj).arg(rs_register::lvalue),
						ctx.file,
						new_kw.line,
						new_kw.col,
						t.lines[new_kw.line]
					);

					instructions.append(
						instruction(rs_instruction::call).arg(proto->constructor()->function_id),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);

					instructions.append(
						instruction(rs_instruction::popState).arg(rs_register::lvalue),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);
				}
				else {
					instructions.append(
						instruction(rs_instruction::newObj),
						ctx.file,
						new_kw.line,
						new_kw.col,
						t.lines[new_kw.line]
					);

					instructions.append(
						instruction(rs_instruction::addProto).arg(rs_register::lvalue).arg(proto->id()),
						ctx.file,
						new_kw.line,
						new_kw.col,
						t.lines[new_kw.line]
					);
				}
			}
			else {
				t.backup_state();
				var_ref r(rs_register::null_register, token(), false);
				r.id = proto->id();
				r.is_const = true;
				r.name = identifier;
				r.reg = rs_register::null_register;

				bool did_push_state = false;
				is_const = compile_identifier(r, identifier, t, ctx, instructions, did_push_state);
				
				if (compile_parameter_list(t, ctx, instructions, false)) {
					// call to function stored in static class variable
					is_const = true;

					instructions.append(
						instruction(rs_instruction::call).arg(rs_register::lvalue),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);

					instructions.append(
						instruction(rs_instruction::move).arg(rs_register::lvalue).arg(rs_register::return_value),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);

					instructions.append(
						instruction(rs_instruction::popState).arg(rs_register::lvalue),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);
				}

				if (did_push_state) {
					instructions.append(
						instruction(rs_instruction::popState).arg(rs_register::lvalue),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);
				}
			}
		}
		*/
	}

	bool script_compiler::compile_expression_value(rs_register destination, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected, bool& value_is_const) {
		if (!expected) {
			t.backup_state();
			instructions.backup();
		}

		auto post_compile = [&t, &ctx, &instructions, &destination, &value_is_const, this]() {
			if (this->compile_accessor_chain(destination, t, ctx, instructions, false)) value_is_const = false;
		};

		token subexpr_open = t.character('(', false);
		if (subexpr_open.valid()) {
			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				subexpr_open.line,
				subexpr_open.col,
				t.lines[subexpr_open.line]
			);

			compile_expression(t, ctx, instructions, true, destination);

			token subexpr_close = t.character(')');

			instructions.append(
				instruction(rs_instruction::popState).arg(destination),
				ctx.file,
				subexpr_close.line,
				subexpr_close.col,
				t.lines[subexpr_close.line]
			);

			if (!expected) {
				t.commit_state();
				instructions.commit();
			}

			value_is_const = true;
			post_compile();
			return true;
		}

		token new_kw = t.keyword(false, "new");
		if (new_kw.valid()) {
			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				new_kw.line,
				new_kw.col,
				t.lines[new_kw.line]
			);

			instructions.append(
				instruction(rs_instruction::newObj).arg(destination),
				ctx.file,
				new_kw.line,
				new_kw.col,
				t.lines[new_kw.line]
			);

			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::this_obj).arg(destination),
				ctx.file,
				new_kw.line,
				new_kw.col,
				t.lines[new_kw.line]
			);

			ctx.constructingPrototype = true;

			bool ignore = false;
			compile_expression_value(rs_register::rvalue, t, ctx, instructions, true, ignore);

			ctx.constructingPrototype = false;

			instructions.append(
				instruction(rs_instruction::move).arg(destination).arg(rs_register::this_obj),
				ctx.file,
				new_kw.line,
				new_kw.col,
				t.lines[new_kw.line]
			);

			instructions.append(
				instruction(rs_instruction::popState).arg(destination),
				ctx.file,
				new_kw.line,
				new_kw.col,
				t.lines[new_kw.line]
			);

			if (!expected) {
				t.commit_state();
				instructions.commit();
			}
			value_is_const = true;
			post_compile();
			return true;
		}

		token func_kw = t.keyword(false, "function");
		if (func_kw.valid()) {
			compile_function(t, ctx, instructions, destination, false);

			if (!expected) {
				t.commit_state();
				instructions.commit();
			}

			value_is_const = true;
			post_compile();
			return true;
		}

		token identifier = t.identifier(false);
		if (identifier.valid()) {
			var_ref var = ctx.var(identifier.text);
			if (var.id || var.reg != rs_register::null_register) {
				if (var.id) {
					instructions.append(
						instruction(rs_instruction::move).arg(destination).arg(var.id),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);
				} else {
					instructions.append(
						instruction(rs_instruction::move).arg(destination).arg(var.reg),
						ctx.file,
						identifier.line,
						identifier.col,
						t.lines[identifier.line]
					);
				}

				if (!expected) {
					t.commit_state();
					instructions.commit();
				}

				value_is_const = var.is_const;
				post_compile();
				return true;
			}

			variable_id func = ctx.func(identifier.text);
			if (func) {
				instructions.append(
					instruction(rs_instruction::move).arg(destination).arg(func),
					ctx.file,
					identifier.line,
					identifier.col,
					t.lines[identifier.line]
				);

				if (!expected) {
					t.commit_state();
					instructions.commit();
				}

				value_is_const = true;
				post_compile();
				return true;
			}

			object_prototype* proto = ctx.proto(identifier.text);
			if (proto) {
				instructions.append(
					instruction(rs_instruction::move).arg(destination).arg(proto->id()),
					ctx.file,
					identifier.line,
					identifier.col,
					t.lines[identifier.line]
				);

				if (!expected) {
					t.commit_state();
					instructions.commit();
				}

				value_is_const = true;
				post_compile();
				return true;
			}

			if (expected) {
				throw parse_exception(
					format("Use of undeclared identifier '%s'", identifier.text.c_str()),
					ctx.file,
					t.lines[identifier.line],
					identifier.line,
					identifier.col
				);
			} else {
				t.restore_state();
				instructions.restore();
			}
			return false;
		}

		if (compile_array(destination, t, ctx, instructions, false)) {
			if (!expected) {
				t.commit_state();
				instructions.commit();
			}

			value_is_const = true;
			post_compile();
			return true;
		}

		token const_token = t.number_constant(false);
		if (const_token.valid()) {
			variable_id var = define_static_number(m_script_context, const_token, ctx, t);

			instructions.append(
				instruction(rs_instruction::move).arg(destination).arg(var),
				ctx.file,
				identifier.line,
				identifier.col,
				t.lines[identifier.line]
			);

			if (!expected) {
				t.commit_state();
				instructions.commit();
			}

			value_is_const = true;
			post_compile();
			return true;
		}
		else {
			const_token = t.string_constant(false, true);
			if (const_token.valid()) {
				variable_id var = define_static_string(m_script_context, const_token.text);

				instructions.append(
					instruction(rs_instruction::move).arg(destination).arg(var),
					ctx.file,
					identifier.line,
					identifier.col,
					t.lines[identifier.line]
				);

				if (!expected) {
					t.commit_state();
					instructions.commit();
				}

				value_is_const = true;
				post_compile();
				return true;
			}
			else {
				if (!compile_json(destination, t, ctx, instructions, false)) {
					if (expected) {
						t.restore_state();
						instructions.restore();
						throw parse_exception(
							"Expected expression",
							ctx.file,
							t.lines[t.line()],
							t.line(),
							t.col()
						);
					} else {
						t.restore_state();
						instructions.restore();
					}

					value_is_const = true;
					post_compile();
					return false;
				} else {
					if (!expected) {
						t.commit_state();
						instructions.commit();
					}

					return true;
				}
			}
		}

		if (!expected) {
			t.restore_state();
			instructions.restore();
		}

		return false;
	}

	bool script_compiler::compile_accessor_chain(rs_register destination, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool is_nested) {
		token prop_access = t.character('.', false);
		if (prop_access.valid()) {
			if (!is_nested) {
				instructions.append(
					instruction(rs_instruction::pushState),
					ctx.file,
					prop_access.line,
					prop_access.col,
					t.lines[prop_access.line]
				);
			}

			token prop_name = t.identifier();

			t.backup_state();
			token assign = t.keyword(false, "=");
			t.restore_state();

			if (!ctx.constructingPrototype) {
				instructions.append(
					instruction(rs_instruction::move).arg(rs_register::this_obj).arg(destination),
					ctx.file,
					prop_name.line,
					prop_name.col,
					t.lines[prop_name.line]
				);
			}

			instructions.append(
				instruction(assign.valid() ? rs_instruction::propAssign : rs_instruction::prop).arg(destination).arg(define_static_string(ctx.ctx, prop_name.text)),
				ctx.file,
				assign.valid() ? assign.line : prop_name.line,
				assign.valid() ? assign.col : prop_name.col,
				t.lines[assign.valid() ? assign.line : prop_name.line]
			);

			if (!assign.valid()) compile_accessor_chain(destination, t, ctx, instructions, true);

			if (!is_nested) {
				instructions.append(
					instruction(rs_instruction::popState).arg(destination),
					ctx.file,
					prop_access.line,
					prop_access.col,
					t.lines[prop_access.line]
				);
			}

			return true;
		}

		token prop_index = t.character('[', false);
		if (prop_index.valid()) {
			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				prop_index.line,
				prop_index.col,
				t.lines[prop_index.line]
			);

			compile_expression(t, ctx, instructions, true, rs_register::rvalue);

			prop_index = t.character(']');

			instructions.append(
				instruction(rs_instruction::popState).arg(rs_register::rvalue),
				ctx.file,
				prop_index.line,
				prop_index.col,
				t.lines[prop_index.line]
			);

			t.backup_state();
			token assign = t.keyword(false, "=");
			t.restore_state();

			if (!ctx.constructingPrototype) {
				instructions.append(
					instruction(rs_instruction::move).arg(rs_register::this_obj).arg(destination),
					ctx.file,
					prop_index.line,
					prop_index.col,
					t.lines[prop_index.line]
				);
			}

			instructions.append(
				instruction(assign.valid() ? rs_instruction::propAssign : rs_instruction::prop).arg(destination).arg(rs_register::rvalue),
				ctx.file,
				assign.valid() ? assign.line : prop_index.line,
				assign.valid() ? assign.col : prop_index.col,
				t.lines[assign.valid() ? assign.line : prop_index.line]
			);

			if (!assign.valid()) compile_accessor_chain(destination, t, ctx, instructions, is_nested);

			return true;
		}

		// maybe function call?
		t.backup_state();
		token par_open = t.character('(', false);
		t.restore_state();

		if (this->compile_parameter_list(t, ctx, instructions, par_open.valid())) {
			if (ctx.constructingPrototype) {
				instructions.append(
					instruction(rs_instruction::addProto).arg(rs_register::this_obj).arg(destination),
					ctx.file,
					par_open.line,
					par_open.col,
					t.lines[par_open.line]
				);
			}

			instructions.append(
				instruction(rs_instruction::call).arg(destination),
				ctx.file,
				par_open.line,
				par_open.col,
				t.lines[par_open.line]
			);

			if (!ctx.constructingPrototype) {
				instructions.append(
					instruction(rs_instruction::move).arg(destination).arg(rs_register::return_value),
					ctx.file,
					par_open.line,
					par_open.col,
					t.lines[par_open.line]
				);

				instructions.append(
					instruction(rs_instruction::popState).arg(destination),
					ctx.file,
					par_open.line,
					par_open.col,
					t.lines[par_open.line]
				);
			} else {
				instructions.append(
					instruction(rs_instruction::popState),
					ctx.file,
					par_open.line,
					par_open.col,
					t.lines[par_open.line]
				);
			}

			return compile_accessor_chain(destination, t, ctx, instructions, false);
		}

		return false;
	}

	bool script_compiler::compile_statement(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool parseSemicolon) {
		t.backup_state();

		auto kw = t.keyword(false);
		if (kw.valid()) {
			if (kw.text == "return") {
				if (!ctx.currentFunction) {
					throw parse_exception(
						"Encountered unexpected return statement",
						ctx.file,
						t.lines[kw.line],
						kw.line,
						kw.col
					);
				}

				if (compile_expression(t, ctx, instructions, false)) {
					instructions.append(
						instruction(rs_instruction::move).arg(rs_register::return_value).arg(rs_register::rvalue),
						ctx.file,
						kw.line,
						kw.col,
						t.lines[kw.line]
					);
				} else {
					instructions.append(
						instruction(rs_instruction::move).arg(rs_register::return_value).arg(variable_id(0)),
						ctx.file,
						kw.line,
						kw.col,
						t.lines[kw.line]
					);
				}
				instructions.append(
					instruction(rs_instruction::ret),
					ctx.file,
					kw.line,
					kw.col,
					t.lines[kw.line]
				);
				if (parseSemicolon) t.character(';');

				ctx.currentFunction->has_explicit_return = true;
			}
			else if (kw.text == "if") {
				ctx.current_scope_idx++;
				t.character('(');
				compile_expression(t, ctx, instructions, true);
				t.character(')');

				integer_type pass_address = instructions.count() + 1;
				variable_id pass_addr_id = define_static_number(m_script_context, pass_address);
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

				token final_token;
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
					final_token = closed;

					instructions.append(
						instruction(rs_instruction::popScope),
						ctx.file,
						closed.line,
						closed.col,
						t.lines[closed.line]
					);

					integer_type fail_address = instructions.count() + 1;
					variable_id fail_addr_id = define_static_number(m_script_context, fail_address);
					branch.arg(fail_addr_id);
					ctx.pop_locals();
				} else {
					compile_statement(t, ctx, instructions, false);
					if (parseSemicolon) {
						final_token = t.semicolon();
						instructions.append(
							instruction(rs_instruction::popScope),
							ctx.file,
							final_token.line,
							final_token.col,
							t.lines[final_token.line]
						);
					} else {
						final_token.line = t.line();
						final_token.col = t.col();

						instructions.append(
							instruction(rs_instruction::popScope),
							ctx.file,
							t.line(),
							t.col(),
							t.lines[t.line()]
						);
					}

					integer_type fail_address = instructions.count() + 1;
					variable_id fail_addr_id = define_static_number(m_script_context, fail_address);
					branch.arg(fail_addr_id);
				}

				instruction& jump_past_else = instructions.append(
					instruction(rs_instruction::null_instruction),
					ctx.file,
					final_token.line,
					final_token.col,
					t.lines[final_token.line]
				);
				token else_kw = t.keyword(false, "else");
				if (else_kw.valid()) {
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
								"Encountered unexpected end of input while parsing 'else' statement body",
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

						integer_type after_pass_address = instructions.count() + 1;
						variable_id after_pass_address_id = define_static_number(m_script_context, after_pass_address);
						jump_past_else.arg(after_pass_address_id);
						jump_past_else.code = rs_instruction::jump;
						ctx.pop_locals();
					} else {
						compile_statement(t, ctx, instructions, false);

						if (parseSemicolon) {
							auto sc = t.semicolon();
							instructions.append(
								instruction(rs_instruction::popScope),
								ctx.file,
								sc.line,
								sc.col,
								t.lines[sc.line]
							);
						} else {
							instructions.append(
								instruction(rs_instruction::popScope),
								ctx.file,
								t.line(),
								t.col(),
								t.lines[t.line()]
							);
						}

						integer_type after_pass_address = instructions.count();
						variable_id after_pass_address_id = define_static_number(m_script_context, after_pass_address);
						jump_past_else.arg(after_pass_address_id);
						jump_past_else.code = rs_instruction::jump;
					}
				}

				ctx.current_scope_idx--;
			}
			else if (kw.text == "while") {
				ctx.current_scope_idx++;
				integer_type expr_address = instructions.count() + 1;
				variable_id expr_addr_id = define_static_number(m_script_context, expr_address);

				t.character('(');
				compile_expression(t, ctx, instructions, true);
				t.character(')');

				integer_type pass_address = instructions.count() + 1;
				variable_id pass_addr_id = define_static_number(m_script_context, pass_address);

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

				integer_type fail_address = instructions.count();
				variable_id fail_addr_id = define_static_number(m_script_context, fail_address);
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
				integer_type expr_address = instructions.count();
				variable_id expr_addr_id = define_static_number(m_script_context, expr_address);

				sc = t.semicolon(false);
				if (!sc.valid()) {
					compile_expression(t, ctx, instructions, true);
					t.semicolon();
				}

				integer_type pass_address = instructions.count() + 1;
				variable_id pass_addr_id = define_static_number(m_script_context, pass_address);

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

					integer_type fail_address = instructions.count();
					variable_id fail_addr_id = define_static_number(m_script_context, fail_address);
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

					integer_type fail_address = instructions.count();
					variable_id fail_addr_id = define_static_number(m_script_context, fail_address);
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
				
				check_declaration(t, ctx, var_name);
				
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
				
				check_declaration(t, ctx, var_name);

				auto ref = var_ref(m_script_context, var_name, false);
				ctx.locals.back().push_back(ref);
				if (ctx.currentFunction) ctx.currentFunction->declared_vars.push_back(ref);
				else if (ctx.current_scope_idx == 0) m_script_context->global_variables.push_back({ ref.id, ref.name, ref.is_const });

				if (!uninitialized) compile_expression(t, ctx, instructions, true);
				if (parseSemicolon) t.character(';');
			}
			else if (kw.text == "else") {
				throw parse_exception(
					"Encountered 'else' without an 'if'",
					ctx.file,
					t.lines[kw.line],
					kw.line,
					kw.col
				);
			}
			else if (kw.text == "class") {
				if (ctx.currentFunction) {
					throw parse_exception(
						"Classes can only be defined at the global scope",
						ctx.file,
						t.lines[kw.line],
						kw.line,
						kw.col
					);
				}
				object_prototype* proto = compile_class(t, ctx, instructions);
				ctx.prototypes.push_back(proto);
			}
			else if (kw.text == "function") {
				compile_function(t, ctx, instructions);
			}
			else if (kw.text == "new") {
				t.restore_state();
				bool compiled = compile_expression(t, ctx, instructions, true);
				if (parseSemicolon) t.character(';');

				return compiled;
			}
			else false;

			return true;
		}

		t.restore_state();
		bool compiled = compile_expression(t, ctx, instructions, true);
		if (parseSemicolon) t.character(';');

		return compiled;
	}

	bool script_compiler::compile_identifier(const var_ref& variable, const tokenizer::token& reference, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool& pushed_state) {
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
			variable_id vid = define_static_string(m_script_context, prop_name.text);

			if (!pushed_state) {
				instructions.append(
					instruction(rs_instruction::pushState),
					ctx.file,
					accessor_operator.line,
					accessor_operator.col,
					t.lines[accessor_operator.line]
				);
				pushed_state = true;
			}

			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::this_obj).arg(rs_register::lvalue),
				ctx.file,
				accessor_operator.line,
				accessor_operator.col,
				t.lines[accessor_operator.line]
			);

			instruction& prop = instructions.append(
				instruction(rs_instruction::prop).arg(rs_register::lvalue).arg(vid),
				ctx.file,
				prop_name.line,
				prop_name.col,
				t.lines[prop_name.line]
			);


			t.backup_state();
			token assignment = t.keyword(false, "=");
			t.restore_state();
			if (assignment.valid()) {
				// this property on the object is being assigned
				prop.code = rs_instruction::propAssign;
			} else {
				compile_identifier(var_ref(m_script_context, tokenizer::token(), false), tokenizer::token(), t, ctx, instructions, pushed_state);
				// lvalue will now be pointing to the accessed variable at the
				// end of the accessor path
			}

			return false;
		}

		auto index_open_operator = t.character('[', false);
		if (index_open_operator.valid()) {
			if (!pushed_state) {
				instructions.append(
					instruction(rs_instruction::pushState),
					ctx.file,
					accessor_operator.line,
					accessor_operator.col,
					t.lines[accessor_operator.line]
				);
				pushed_state = true;
			}

			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::this_obj).arg(rs_register::lvalue),
				ctx.file,
				accessor_operator.line,
				accessor_operator.col,
				t.lines[accessor_operator.line]
			);

			auto num = t.number_constant(false);
			if (num.valid()) {
				variable_id vid = define_static_string(m_script_context, num.text);
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
				auto str = t.string_constant(false, true, false);
				if (str.valid()) {
					variable_id vid = define_static_string(m_script_context, str.text);

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
						instruction(rs_instruction::popState).arg(rs_register::rvalue),
						ctx.file,
						index_open_operator.line,
						index_open_operator.col,
						t.lines[index_open_operator.line]
					);

					instruction& prop = instructions.append(
						instruction(rs_instruction::prop).arg(rs_register::lvalue).arg(rs_register::rvalue),
						ctx.file,
						index_open_operator.line,
						index_open_operator.col,
						t.lines[index_open_operator.line]
					);

					t.backup_state();
					token assignment = t.keyword(false, "=");
					t.restore_state();

					if (assignment.valid()) {
						// this property on the object is being assigned
						prop.code = rs_instruction::propAssign;
					}
				}
			}

			return false;
		}

		return variable.is_const;
	}

	bool script_compiler::compile_parameter_list(tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected) {
		if (!expected) {
			t.backup_state();
			instructions.backup();
		}

		token popen = t.character('(', expected);
		if (!popen.valid()) {
			t.restore_state();
			instructions.restore();
			return false;
		}

		instructions.append(
			instruction(rs_instruction::pushState),
			ctx.file,
			popen.line,
			popen.col,
			t.lines[popen.line]
		);

		instructions.append(
			instruction(rs_instruction::clearParams),
			ctx.file,
			popen.line,
			popen.col,
			t.lines[popen.line]
		);

		token pclose;
		token comma;
		for (u16 p = 0;p < 128;p++) {
			if (p >= 8) {
				throw parse_exception(
					"Functions can only have up to 8 parameters",
					ctx.file,
					t.lines[comma.line],
					comma.line,
					comma.col
				);
			}

			pclose = t.character(')', !comma.valid() && p > 0);
			if (pclose.valid()) break;

			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				popen.line,
				popen.col,
				t.lines[popen.line]
			);

			compile_expression(t, ctx, instructions, true, rs_register(rs_register::parameter0 + p));
			comma = t.character(',', false);

			instructions.append(
				instruction(rs_instruction::popState).arg(rs_register(rs_register::parameter0 + p)),
				ctx.file,
				(comma.valid() ? comma : pclose).line,
				(comma.valid() ? comma : pclose).col,
				t.lines[(comma.valid() ? comma : pclose).line]
			);
		}

		if (!expected) {
			t.commit_state();
			instructions.commit();
		}
		return true;
	}

	bool script_compiler::compile_json(rs_register destination, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected) {
		t.backup_state();
		token obj_open = t.character('{', expected);
		if (!obj_open.valid()) {
			t.restore_state();
			return false;
		}

		instructions.append(
			instruction(rs_instruction::pushState),
			ctx.file,
			obj_open.line,
			obj_open.col,
			t.lines[obj_open.line]
		);

		instructions.append(
			instruction(rs_instruction::newObj),
			ctx.file,
			obj_open.line,
			obj_open.col,
			t.lines[obj_open.line]
		);

		token obj_close = t.character('}', false);
		bool maybe_not_json = !expected;
		while (!t.at_end() && !obj_close.valid()) {
			token propName = t.identifier(false);
			if (!propName.valid()) propName = t.number_constant(false);
			if (!propName.valid()) propName = t.string_constant(!maybe_not_json, true, false);
			if (!propName.valid()) {
				// can only happen if this isn't a JSON object
				t.restore_state();
				return false;
			}

			variable_id prop_name_id = define_static_string(m_script_context, propName.text);

			token assign = t.character(':', !maybe_not_json);
			if (!assign.valid()) {
				// can only happen if this isn't a JSON object
				t.restore_state();
				return false;
			}

			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				assign.line,
				assign.col,
				t.lines[assign.line]
			);

			compile_expression(t, ctx, instructions, true);

			instructions.append(
				instruction(rs_instruction::popState).arg(rs_register::rvalue),
				ctx.file,
				assign.line,
				assign.col,
				t.lines[assign.line]
			);

			instructions.append(
				instruction(rs_instruction::pushState),
				ctx.file,
				assign.line,
				assign.col,
				t.lines[assign.line]
			);
			instructions.append(
				instruction(rs_instruction::propAssign).arg(rs_register::lvalue).arg(prop_name_id),
				ctx.file,
				assign.line,
				assign.col,
				t.lines[assign.line]
			);
			instructions.append(
				instruction(rs_instruction::store).arg(rs_register::lvalue).arg(rs_register::rvalue),
				ctx.file,
				assign.line,
				assign.col,
				t.lines[assign.line]
			);
			instructions.append(
				instruction(rs_instruction::popState),
				ctx.file,
				assign.line,
				assign.col,
				t.lines[assign.line]
			);

			obj_close = t.character('}', false);
		}

		if (!obj_close.valid()) {
			throw parse_exception(
				"Encountered unexpected end of input while parsing object body",
				ctx.file,
				t.lines[obj_open.line],
				obj_open.line,
				obj_open.col
			);
		}

		if (destination != rs_register::lvalue) {
			instructions.append(
				instruction(rs_instruction::move).arg(destination).arg(rs_register::lvalue),
				ctx.file,
				obj_close.line,
				obj_close.col,
				t.lines[obj_close.line]
			);
		}

		instructions.append(
			instruction(rs_instruction::popState).arg(destination),
			ctx.file,
			obj_close.line,
			obj_close.col,
			t.lines[obj_close.line]
		);

		t.commit_state();
		return true;
	}

	bool script_compiler::compile_array(rs_register destination, tokenizer& t, parse_context& ctx, instruction_array& instructions, bool expected) {
		token arr_open = t.character('[', expected);
		if (!arr_open.valid()) {
			return false;
		}

		instructions.append(
			instruction(rs_instruction::pushState),
			ctx.file,
			arr_open.line,
			arr_open.col,
			t.lines[arr_open.line]
		);

		instructions.append(
			instruction(rs_instruction::newArr).arg(rs_register::this_obj),
			ctx.file,
			arr_open.line,
			arr_open.col,
			t.lines[arr_open.line]
		);

		token arr_close = t.character(']', false);
		token comma;
		variable_id push_id = define_static_string(m_script_context, "push");
		while (!t.at_end() && !arr_close.valid()) {
			token rel_token = comma.valid() ? comma : arr_open;

			compile_expression(t, ctx, instructions, true);
			comma = t.character(',', false);

			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::lvalue).arg(rs_register::this_obj),
				ctx.file,
				rel_token.line,
				rel_token.col,
				t.lines[rel_token.line]
			);
			instructions.append(
				instruction(rs_instruction::prop).arg(rs_register::lvalue).arg(push_id),
				ctx.file,
				rel_token.line,
				rel_token.col,
				t.lines[rel_token.line]
			);
			instructions.append(
				instruction(rs_instruction::move).arg(rs_register::parameter0).arg(rs_register::rvalue),
				ctx.file,
				rel_token.line,
				rel_token.col,
				t.lines[rel_token.line]
			);
			instructions.append(
				instruction(rs_instruction::call).arg(rs_register::lvalue),
				ctx.file,
				rel_token.line,
				rel_token.col,
				t.lines[rel_token.line]
			);

			arr_close = t.character(']', !comma.valid());
		}

		if (!arr_close.valid()) {
			throw parse_exception(
				"Encountered unexpected end of input while parsing array body",
				ctx.file,
				t.lines[arr_close.line],
				arr_close.line,
				arr_close.col
			);
		}

		if (destination != rs_register::this_obj) {
			instructions.append(
				instruction(rs_instruction::move).arg(destination).arg(rs_register::this_obj),
				ctx.file,
				arr_close.line,
				arr_close.col,
				t.lines[arr_close.line]
			);
		}

		instructions.append(
			instruction(rs_instruction::popState).arg(destination),
			ctx.file,
			arr_close.line,
			arr_close.col,
			t.lines[arr_close.line]
		);

		return true;
	}
};