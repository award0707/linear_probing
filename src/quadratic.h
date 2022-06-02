#ifndef QUADRATIC_H
#define QUADRATIC_H

#include <string>
#include "hashtable.h"

class quadratic : public hashtable {
	private:
		bool insert(record *t, int key, int value, bool rebuilding);

	public:
		quadratic(std::size_t b) : hashtable(b) { }
		~quadratic() { }
		std::string table_type() { return "quadratic"; }

		using hashtable::insert;
		bool query(int key, int *value);
		bool remove(int key);
};

#endif
