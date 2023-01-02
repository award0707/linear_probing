#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>

class hashtable {
	protected:
		enum slot_state { FULL, EMPTY, DELETED };
		enum optype { INSERT, QUERY, REMOVE, REBUILD };

		struct record { 
			int key, value;
			slot_state state;		
		} *table;

		std::size_t buckets;
		std::size_t records;
		
		int prime_index;
		double max_load_factor;

		uint64_t search_count;
		double miss_running_avg;

		virtual bool insert(record *t, int key,
							int value, bool rebuilding) = 0;
		int hash(int k);
		void update_misses(int misses, enum optype op);

	public:
		hashtable(std::size_t b);
		~hashtable();
		virtual std::string table_type() { return "unspecified"; } 

		virtual bool query(int key, int *value) = 0;
		virtual bool remove(int key) = 0;
		bool insert(int key, int value) {
			return insert(table,key,value,false);
		}
		
		void resize(std::size_t);
		void set_max_load_factor(double f);
		
		// Performance characteristics
		uint64_t total_misses;
		uint64_t inserts, queries, removes;
		uint64_t insert_misses, query_misses, remove_misses;
		uint64_t rebuild_inserts, rebuild_insert_misses;
		uint64_t failed_inserts, failed_queries, failed_removes, duplicates;
		uint64_t resizes;
		uint64_t longest_search;

		virtual void reset_perf_counts();
		virtual void report_testing_stats(std::ostream &os = std::cout,
										  bool verbose = true);

	    // return cluster length data, with clusters bounded either by
		// empty slots and by tombstones 
		void cluster_len(std::vector<int> &empty_clust,
						 std::vector<int> &tomb_clust);

		double load_factor();
		double avg_misses();
		std::size_t table_size();
		std::size_t table_size_bytes();
		std::size_t num_records();
};
#endif
