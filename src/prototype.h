#pragma once
#include <defs.h>
#include <dynamic_array.hpp>
#include <parse_utils.h>
#include <string>

namespace rs {
	class script_function;
	class script_object;
	class object_prototype {
		public:
			object_prototype(context* ctx, const std::string& name);
			object_prototype(context* ctx, const tokenizer::token& declaration);
			~object_prototype();

			script_object* create(context* ctx, variable_id* args, u8 arg_count);

			inline const std::string name() const { return m_declaration.text; }
			inline const tokenizer::token declaration() { return m_declaration; }
			inline const type_id type() const { return m_type_id; }
			inline const variable_id id() const { return m_id; }

			inline script_function* constructor() { return m_constructor; }
			script_function* method(const std::string& name);
			script_function* static_method(const std::string& name);
			variable_id static_variable(const std::string& name);

			object_prototype* constructor(script_function* func);
			object_prototype* method(script_function* func);
			object_prototype* static_method(script_function* func);
			object_prototype* static_variable(const std::string& name, variable_id id);

			object_prototype* parent;

		protected:
			variable_id m_id;
			type_id m_type_id;
			tokenizer::token m_declaration;
			associative_pod_array<std::string, script_function*> m_methods;
			associative_pod_array<std::string, script_function*> m_static_methods;
			associative_pod_array<std::string, variable_id> m_static_vars;
			script_function* m_constructor;
	};
};