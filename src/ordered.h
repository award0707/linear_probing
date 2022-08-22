#ifndef ORDERED_H
#define ORDERED_H

#include <string>
#include "hashtable.h"

class ordered : public hashtable {
	private:
		bool insert(record *t, int key, int value, bool rebuilding);
		void reset_rebuild_window();		
		int rebuild_window;
		int table_start;

	public:
		ordered(std::size_t b);
		~ordered() { }
		std::string table_type() { return "orderedlinear"; }

		using hashtable::insert;
		bool query(int key, int *value);
		bool remove(int key);
		void rebuild();
		
		// statistics
		int rebuilds;
		void reset_perf_counts();
		void report_testing_stats(std::ostream &os = std::cout);

		// debugging
		friend void dump(ordered &);
		bool check_ordering();
		bool disable_rebuilds;
};

#endif
