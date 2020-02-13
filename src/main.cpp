#include <stdio.h>

#include <execution_state.h>
#include <context.h>

std::string var_to_string(rs::context_memory::mem_var& v) {
	std::string val;
	if (!v.data) val = "undefined";
	else {
		switch (v.type) {
			case rs::rs_builtin_type::t_string: {
				val = std::string((char*)v.data, v.size);
				break;
			}
			case rs::rs_builtin_type::t_int8: {
				val = rs::format("%d", *(char*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint8: {
				val = rs::format("%u", *(unsigned char*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_int16: {
				val = rs::format("%d", *(short*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint16: {
				val = rs::format("%u", *(unsigned short*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_int32: {
				val = rs::format("%d", *(rs::i32*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint32: {
				val = rs::format("%u", *(rs::u32*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_int64: {
				val = rs::format("%ll", *(rs::i64*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_uint64: {
				val = rs::format("%llu", *(rs::u64*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_float: {
				val = rs::format("%f", *(float*)v.data);
				break;
			}
			case rs::rs_builtin_type::t_double: {
				val = rs::format("%f", *(double*)v.data);
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
				val = "object";
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

int main(int arg_count, const char** args) {
	for(int i = 0;i < arg_count;i++) {
		printf("args[%d]: %s\n", i, args[i]);
	}

	rs::context_parameters p;
	rs::context ctx(p);
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

	/*
	const char* instructions[] = {
		"null",
		"store",
		"prop",
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
		"inc",
		"dec",
		"branch",
		"call",
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
	* /
	
	rs::context_memory::mem_var result = { 0 };
	ctx.execute("x(1, t(), 3);", result);
	printf("x(1, t(), 3) : %s\n", var_to_string(result).c_str());
	*/

	char input[1024] = { 0 };
	system("cls");
	rs::context_memory::mem_var result = { 0 };
	while (input[0] != 'q') {
		if (result.data) delete [] result.data;
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