#ifndef LINEAR_H
#define LINEAR_H

#include <string>
#include "hashtable.h"

class linear : public hashtable {
	private:
		void reset_rebuild_window();
		bool insert(record *t, int key, int value, bool rebuilding);
		int rebuild_window;
		int tombs;

	public:
		linear(std::size_t b);
		~linear() { }
		std::string table_type() { return "linear"; }

		using hashtable::insert;
		bool query(int key, int *value);
		bool remove(int key);
		void rebuild();

		// statistics
		int rebuilds;
		
		// debugging
		friend void dump(linear &);
		bool disable_rebuilds;
		bool check_ordering() { return true; }
};

#endif
