#ifndef GRAVEYARD_H 
#define GRAVEYARD_H

#include <string>
#include "hashtable.h"

class graveyard : public hashtable {
	private:
		bool insert(record *t, int key, int value, bool rebuilding);
		void insert_tombstone(int key);
		void reset_rebuild_window();		
		int rebuild_window;
		int table_start;

	public:
		graveyard(std::size_t b);
		~graveyard() { }
		std::string table_type() { return "graveyard"; }

		using hashtable::insert;
		bool query(int key, int *value);
		bool remove(int key);
		void rebuild();

		// statistics
		int rebuilds;
		void reset_perf_counts();
		void report_testing_stats(std::ostream &os = std::cout);

		// debugging
		friend void dump(graveyard &);
		bool check_ordering();
		bool disable_rebuilds;
};

#endif
