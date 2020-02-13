#include <dynamic_array.hpp>
#ifndef DONT_DECLARE_DYNAMIC_ARRAY
namespace rs {
	void* r2realloc(void* data, size_t old_size, size_t size) {
		u8* newData = new u8[size];
		memset(newData + old_size, 0, size - old_size);
		if (old_size > 0) memcpy(newData, data, old_size);
		delete [] data;
		return newData;
	}
};
#endif