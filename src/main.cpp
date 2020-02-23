#include <stdio.h>

#include <execution_state.h>
#include <context.h>
#include <script_object.h>
#include <script_function.h>

/*
class test : public rs::script_object {
	public:
		test(rs::context* ctx) : rs::script_object(ctx) {
			inject_property(this, "value", rs::rs_builtin_type::t_integer, &test::value);

			rs::variable_id t_id = ctx->memory->inject(rs::rs_builtin_type::t_object, sizeof(test*), this);
			rs::tokenizer::token tok = { __LINE__, 0, "test", __FILE__ }; 
			ctx->global_variables.push_back({ t_id, tok, false });

			value = 45;
		}
		~test() {
		}

		rs::integer_type value;
};*/

rs::variable test_func(rs::func_args* args) {
	std::string str;
	args->parameters.for_each([&args, &str](rs::variable* arg) {
		if (str.length() > 0) str += " ";
		str += arg->to_string();
		return true;
	});

	rs::variable result(rs::rs_variable_flags::f_const);
	result.set((rs::integer_type)printf("----%s: %s\n", args->self.to_string().c_str(), str.c_str()));
	return result;
}


int main(int arg_count, const char** args) {
	for(int i = 0;i < arg_count;i++) {
		printf("args[%d]: %s\n", i, args[i]);
	}

	rs::context_parameters p;
	rs::context ctx(p);
	ctx.bind_function("test_print", test_func);
	//test t(&ctx);

	ctx.add_code(
		"function t() { return 100; }\n"
		"function x(a, b, c) {\n"
			"const d = 15;\n"
			"let e = 5;\n"
			"e = t();\n"
			"return (d - e) + 5.25 + -500 * (a + b + c);\n"
		"}\n"
		"class Test {"
			"constructor(g) {"
				"this.a = g;"
			"}"
			"test() {"
				"return this.a;"
			"}"
			"static f = 32;"
			"static c() { return 5; }"
		"};\n"
	);

	char input[1024] = { 0 };
	system("cls");
	while (input[0] != 'q') {
		memset(input, 0, 1024);
		fgets(input, 1024, stdin);
		system("cls");
		std::string code = input;
		rs::variable result;
		ctx.execute(code, result);
		printf("result: %s\n", result.to_string().c_str());
	}

	return 0;
}