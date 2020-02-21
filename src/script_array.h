#pragma once
#include <defs.h>
#include <script_object.h>

namespace rs {
	class script_array : public script_object {
		public:
			script_array(context* ctx);
			virtual ~script_array();

			inline variable_id* elements() { return m_elements[0]; }
			inline size_t count() { return m_elements.size(); }

			void push(register_type& value);

		protected:
			dynamic_pod_array<variable_id> m_elements;
	};

	class object_prototype;
	void initialize_array_prototype(context* ctx, object_prototype* prototype);
};