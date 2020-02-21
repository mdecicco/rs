#pragma once
#include <instruction_array.h>

namespace rs {
	class context;
	void add_default_instruction_set(context* ctx);
	void add_number_instruction_set(context* ctx);
	void add_object_instruction_set(context* ctx);
	void add_string_instruction_set(context* ctx);
	void add_class_instruction_set(context* ctx);
	void add_array_instruction_set(context* ctx);
};