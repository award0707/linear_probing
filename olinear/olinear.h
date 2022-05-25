#ifndef __OLINEAR
#define __OLINEAR

#include <cstdint>
#include "primes.h"

enum node_status { FULL, EMPTY, DELETED };
enum optype { INSERT, FIND, REMOVE, REHASH };

struct entry {
	int key;
	int value;
};

struct hashnode {
	struct entry data;
	node_status status;
};

class kvstore {
	private:
		hashnode *table;
		std::size_t buckets;
		std::size_t elements;
		int prime_index;
		double p_max_load_factor;
		int rebuild_window;

		int search_count;
		double avg_find_miss;
		int coll;

		bool insert(hashnode *t, int key, int value, bool rehashing);
		uint64_t hash(int);
		void update_misses(int misses, enum optype op);
		void rebuild();

	public:
		kvstore(int b = 7);
		~kvstore();

		bool get(int key, int* value);
	        bool set(int key, int value);

		bool insert(int key, int value);
		int* find(int key);
		bool remove(int key);
		void rehash();
		void set_max_load_factor(double f);
		
		// Performance characteristics
		int insert_c, find_c, remove_c;
		int insert_coll_c, find_coll_c, remove_coll_c;
		int rehashing_insert_c, rehashing_insert_coll_c;
		int failed_insert, failed_find, failed_remove;
		int rehashes;
		int rebuilds;
		int longest_search;

		void reset_counts();
		double load_factor();
		double avg_misses();
		int bucket_c();
		int collision_c();
		int element_c();
};

#endif
