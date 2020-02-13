#pragma once
#include <instruction_array.h>

namespace rs {
	class context;
	void add_default_instruction_set(context* ctx);
	void add_i32_instruction_set(context* ctx);
};