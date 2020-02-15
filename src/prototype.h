#pragma once
#include <defs.h>
#include <dynamic_array.hpp>
#include <string>

namespace rs {
	class function;
	class object_prototype {
		public:
			object_prototype(const std::string& name);
			~object_prototype();

			inline const std::string name() const { return m_name; }
			inline const type_id type() const { return m_type_id; }

			function* constructor();
			function* method(const std::string& name);
			function* static_method(const std::string& name);
			variable_id static_variable(const std::string& name);

			object_prototype* constructor(function* func);
			object_prototype* method(function* func);
			object_prototype* static_method(function* func);
			object_prototype* static_variable(const std::string& name, variable_id id);



		protected:
			type_id m_type_id;
			std::string m_name;
			associative_pod_array<std::string, function*> m_methods;
			associative_pod_array<std::string, function*> m_static_methods;
			associative_pod_array<std::string, variable_id> m_static_vars;
			function* m_constructor;
	};
};