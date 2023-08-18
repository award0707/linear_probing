#ifndef ORDERED_H
#define ORDERED_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>


class ordered_aos {
	private:
		enum slot_state { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD };

		struct record { 
			int key, value;
			slot_state state;		
		} *table;

		uint64_t buckets;
		uint64_t records;
		uint64_t tombs;
		uint64_t table_head;
		int rebuild_window;
		
		int prime_index;
		double max_load_factor;

		uint64_t search_count;
		double miss_running_avg;

		uint64_t hash(int64_t k);
		bool probe(int k, uint64_t *slot, optype operation, bool* wrapped = NULL);
		uint64_t shift(uint64_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline slot_state state(uint64_t k) { return table[k].state; }
		inline key_t& key(uint64_t k) { return table[k].key; }
		inline value_t& value(uint64_t k) { return table[k].value; }
		inline bool full(uint64_t k) { return state(k) == FULL; }
		inline bool empty(uint64_t k) { return state(k) == EMPTY; }
		inline bool tomb(uint64_t k) { return state(k) == TOMB; }

	public:
		enum insert_result { FAIL_FULL, FAIL_DUP, SUCCESS, NEEDS_REBUILD };

		ordered_aos(uint64_t b);
		~ordered_aos(); 
		std::string table_type() { return "ordered_aos"; }

		void resize(uint64_t);
		void set_max_load_factor(double f) { max_load_factor = f; }
		
		insert_result insert(int key, int value, bool rebuilding = false);
		bool query(int key, int *value);
		bool remove(int key);
		void rebuild();

		// Performance characteristics
		uint64_t total_misses;
		uint64_t inserts, queries, removes;
		uint64_t insert_misses, query_misses, remove_misses;
		uint64_t rebuild_inserts, rebuild_insert_misses;
		uint64_t failed_inserts, failed_queries, failed_removes, duplicates;
		uint64_t resizes;
		uint64_t longest_search;
		uint64_t rebuilds;

		void reset_perf_counts();
		void report_testing_stats(std::ostream &os = std::cout,
				bool verbose = true);

		// return cluster length data, with clusters bounded either by
		// empty slots and by tombstones 
		void cluster_len(std::vector<int> &empty_clust,
				std::vector<int> &tomb_clust);

		double load_factor() { return (double)records/buckets; }
		double avg_misses() { return miss_running_avg; }
		uint64_t table_size() { return buckets; }
		uint64_t table_size_bytes() { return buckets*sizeof(record); }
		uint64_t num_records() { return records; }

		// debugging
		friend void dump(ordered_aos &);
		bool disable_rebuilds;
		bool check_ordering();
};
#endif
