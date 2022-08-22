#ifndef LINEAR_H
#define LINEAR_H

#include <string>
#include "hashtable.h"

class linear : public hashtable {
	private:
		bool insert(record *t, int key, int value, bool rebuilding);

	public:
		linear(std::size_t b) : hashtable(b) { }
		~linear() { }
		std::string table_type() { return "linear"; }

		using hashtable::insert;
		bool query(int key, int *value);
		bool remove(int key);
};

#endif
