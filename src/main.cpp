#include <stdio.h>

#include <execution_state.h>
#include <context.h>
#include <script_object.h>

std::string var_to_string(rs::context_memory::mem_var& v) {
	std::string val;
	if (!v.data) val = "undefined";
	else {
		switch (v.type) {
			case rs::rs_builtin_type::t_string: {
				val = std::string((char*)v.data, v.size);
				break;
			}
			case rs::rs_builtin_type::t_integer: {
				val = rs::format("%d", *(rs::integer_type*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_decimal: {
				val = rs::format("%f", *(rs::decimal_type*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_bool: {
				val = rs::format("%s", (*(bool*)v.data) ? "true" : "false");
				break;
			}
			case rs::rs_builtin_type::t_class: {
				val = "class";
				break;
			}
			case rs::rs_builtin_type::t_function: {
				val = "function";
				break;
			}
			case rs::rs_builtin_type::t_object: {
				rs::script_object* obj = (rs::script_object*)v.data;
				auto properties = obj->properties();
				val = "{ ";

				rs::u32 i = 0;
				for (auto& p : properties) {
					if (i > 0) val += ", ";
					val += p.name + ": " + var_to_string(obj->ctx()->memory->get(p.id));
					i++;
				}


				if (i > 0) val += " ";

				val += "}";

				break;
			}
			default: {
				val = "scripted type";
				break;
			}
		}
	}

	return val;
}

void print_instructions(const rs::context& ctx) {
	const char* instructions[] = {
		"null",
		"store",
		"newObj",
		"prop",
		"propAssign",
		"move",
		"add",
		"sub",
		"mul",
		"div",
		"mod",
		"pow",
		"or",
		"and",
		"less",
		"greater",
		"addEq",
		"subEq",
		"mulEq",
		"divEq",
		"modEq",
		"powEq",
		"orEq",
		"andEq",
		"lessEq",
		"greaterEq",
		"compare",
		"inc",
		"dec",
		"branch",
		"call",
		"jump",
		"ret",
		"pushState",
		"popState",
		"pushScope",
		"popScope"
	};
	const char* registers[] = {
		"null",
		"this_obj",
		"return_val",
		"return_address",
		"instruction_address",
		"lvalue",
		"rvalue",
		"parameter0",
		"parameter1",
		"parameter2",
		"parameter3",
		"parameter4",
		"parameter5",
		"parameter6",
		"parameter7",
		"parameter_count"
	};

	printf("--- raw instructions ---\n");
	for (int i = 0;i < ctx.instructions->count();i++) {
		auto inst = (*ctx.instructions)[i];
		printf("%d: %s", i, instructions[inst.code]);
		for (int a = 0;a < inst.arg_count;a++) {
			if (inst.arg_is_register[a]) printf(" $%s", registers[inst.args[a].reg]);
			else {
				auto& v = ctx.memory->get(inst.args[a].var);
				printf(" #%llu (%s)", inst.args[a].var, var_to_string(v).c_str());
			}
		}
		printf("\n");
	}

	/*
	printf("--- instructions with code ---\n");

	int last_line = -1;
	int last_col = -1;
	for (int i = 0;i < ctx.instructions->count();i++) {
	auto inst = (*ctx.instructions)[i];
	auto src = ctx.instructions->instruction_source(i);
	if (src.line != last_line || src.col != last_col) printf("\n\n%s\n", src.lineText.c_str());
	last_line = src.line;
	last_col = src.col;
	for (int c = 0;c < int(src.col);c++) printf(" ");
	printf("^ ");
	printf(instructions[inst.code]);
	for (int a = 0;a < inst.arg_count;a++) {
	if (inst.arg_is_register[a]) printf(" $%s", registers[inst.args[a].reg]);
	else {
	auto& v = ctx.memory->get(inst.args[a].var);
	printf(" #%llu (%s)", inst.args[a].var, var_to_string(v).c_str());
	}
	}
	printf("\n");
	}
	*/
}

class test : public rs::script_object {
	public:
		test(rs::context* ctx) : rs::script_object(ctx) {
			define_property("value", rs::rs_builtin_type::t_integer, &test::value);

			rs::variable_id t_id = ctx->memory->inject(rs::rs_builtin_type::t_object, sizeof(test*), this);
			rs::tokenizer::token tok = { __LINE__, 0, "test", __FILE__ }; 
			ctx->global_variables.push_back({ t_id, tok, false });

			value = 45;
		}
		~test() {
		}

		rs::integer_type value;
};


int main(int arg_count, const char** args) {
	for(int i = 0;i < arg_count;i++) {
		printf("args[%d]: %s\n", i, args[i]);
	}

	rs::context_parameters p;
	rs::context ctx(p);
	test t(&ctx);

	ctx.add_code(
		"function t() { return 100; }\n"
		"function x(a, b, c) {\n"
			"const d = 15;\n"
			"let e = 5;\n"
			"e = t();\n"
			"return (d - e) + 5.25 + -500 * (a + b + c);\n"
		"}\n"
		"for(let f = 0;f < 10;f++) x(1, t(), 3);\n"
	);

	print_instructions(ctx);

	/*	
	rs::context_memory::mem_var result = { 0 };
	ctx.execute("x(1, t(), 3);", result);
	printf("x(1, t(), 3) : %s\n", var_to_string(result).c_str());
	*/

	printf("press enter to continue\n");
	char c[4] = { 0 };
	fgets(c, 4, stdin);

	char input[1024] = { 0 };
	system("cls");
	rs::context_memory::mem_var result = { 0 };
	while (input[0] != 'q') {
		result.data = nullptr;
		result.size = 0;
		result.type = 0;
		memset(input, 0, 1024);
		fgets(input, 1024, stdin);
		system("cls");
		std::string code = input;
		ctx.execute(code, result);
		printf("result: %s\n", var_to_string(result).c_str());
	}

	return 0;
}