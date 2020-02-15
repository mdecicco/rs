#include <defs.h>
#include <object.h>
#define max(a, b) ((a) > (b) ? (a) : (b))
namespace rs {
	const size_t max_variable_size = max(
		sizeof(script_object*),
		max(
			sizeof(integer_type),
			sizeof(decimal_type)
		)
	);
};