#ifndef LINEAR_H
#define LINEAR_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>


class linear_aos {
	private:
		enum slot_state { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD };

		struct record { 
			int key, value;
			slot_state state;		
		} *table;

		std::size_t buckets;
		std::size_t records;
		std::size_t tombs;
		int rebuild_window;
		
		int prime_index;
		double max_load_factor;

		uint64_t search_count;
		double miss_running_avg;

		uint64_t hash(int k);
		bool probe(int k, uint64_t *slot, optype operation);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline slot_state state(int k) { return table[k].state; }
		inline int key(int k) { return table[k].key; }
		inline int value(int k) { return table[k].value; }
		inline bool full(int k) { return state(k) == FULL; }
		inline bool empty(int k) { return state(k) == EMPTY; }
		inline bool tomb(int k) { return state(k) == TOMB; }

	public:
		enum insert_result { FAIL_FULL, FAIL_DUP, SUCCESS, NEEDS_REBUILD };

		linear_aos(std::size_t b);
		~linear_aos(); 
		std::string table_type() { return "linear_aos"; }

		void resize(std::size_t);
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
		std::size_t table_size() { return buckets; }
		std::size_t table_size_bytes() { return buckets*sizeof(record); }
		std::size_t num_records() { return records; }

		// debugging
		friend void dump(linear_aos &);
		bool disable_rebuilds;
		bool check_ordering() { return true; }
};
#endif
