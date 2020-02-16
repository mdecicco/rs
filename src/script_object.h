#pragma once
#include <defs.h>
#include <context.h>

namespace rs {
	class script_object {
		public:
			script_object(context* ctx);
			virtual ~script_object();

			template<typename T, typename U>
			variable_id define_property(const std::string& name, type_id type, U T::*member);
			variable_id define_property(const std::string& name, type_id type, size_t size, void* data);
			void delete_property(const std::string& name);

			variable_id property(const std::string& name);

			struct prop {
				variable_id id;
				std::string name;
			};
			std::vector<prop> properties() const;
			inline context* ctx() { return m_context; }

		protected:
			std::unordered_map<std::string, variable_id> m_props;
			context* m_context;
	};

	template<typename T, typename U>
	variable_id script_object::define_property(const std::string& name, type_id type, U T::*member) {
		size_t offset = (char*)&((T*)nullptr->*member) - (char*)nullptr;
		variable_id id = m_context->memory->inject(type, sizeof(U), ((u8*)this) + offset);
		m_props[name] = id;
		return id;
	}
};