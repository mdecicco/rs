#include <stdio.h>

#include <execution_state.h>
#include <context.h>
#include <script_object.h>
#include <script_function.h>

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
};

rs::variable_id test_func(rs::func_args* args) {
	std::string str;
	args->parameters.for_each([&args, &str](rs::register_type* arg) {
		rs::mem_var mv = arg->ref(args->context);
		if (str.length() > 0) str += " ";
		str += rs::var_tostring(mv);
		return true;
	});

	rs::mem_var mv;
	memset(&mv, 0, sizeof(mv));
	if (args->self) mv = args->context->memory->get(args->self->id());
	rs::integer_type result = printf("----%s: %s\n", rs::var_tostring(mv).c_str(), str.c_str());
	
	return args->context->memory->set(rs::rs_builtin_type::t_integer, sizeof(rs::integer_type), &result);
}


int main(int arg_count, const char** args) {
	for(int i = 0;i < arg_count;i++) {
		printf("args[%d]: %s\n", i, args[i]);
	}

	rs::context_parameters p;
	rs::context ctx(p);
	ctx.bind_function("test_print", test_func);
	test t(&ctx);

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
	rs::mem_var result = { 0 };
	while (input[0] != 'q') {
		result.data = nullptr;
		result.size = 0;
		result.type = 0;
		memset(input, 0, 1024);
		fgets(input, 1024, stdin);
		system("cls");
		std::string code = input;
		ctx.execute(code, result);
		printf("result: %s\n", var_tostring(result).c_str());
	}

	return 0;
}