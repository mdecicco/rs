#pragma once
#include <defs.h>
#include <context.h>

namespace rs {
	class object_prototype;

	class script_object {
		public:
			script_object(context* ctx);
			virtual ~script_object();

			template<typename T, typename U>
			variable_id inject_property(void* self, const std::string& name, type_id type, U T::*member, bool read_only = false);
			variable_id define_property(const std::string& name, u16 flags = 0);
			void delete_property(const std::string& name);

			variable_id property(const std::string& name);
			variable_id proto_property(const std::string& name);

			void add_prototype(object_prototype* prototype, bool call_constructor, variable_id* args, u8 arg_count);

			struct prop {
				variable_id id;
				std::string name;
			};
			std::vector<prop> properties() const;
			inline context* ctx() { return m_context; }
			inline variable_id id() { return m_id; }
			inline bool set_id(variable_id id) {
				if (!m_id) {
					m_id = id;
					return true;
				}
				return false;
			}

			object_prototype* prototype;

		protected:
			munordered_map<std::string, variable_id> m_props;
			context* m_context;
			variable_id m_id;
	};

	template<typename T, typename U>
	variable_id script_object::inject_property(void* self, const std::string& name, type_id type, U T::*member, bool read_only) {
		return 0;
		/*
		size_t offset = (char*)&((T*)nullptr->*member) - (char*)nullptr;
		variable_id id = m_context->memory->inject(type, sizeof(U), ((u8*)self) + offset);
		m_props[name] = id;
		return id;
		*/
	}
};