#ifndef GRAVEYARD_H
#define GRAVEYARD_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>

class graveyard_aos {
	private:
		typedef int key_t;
		typedef int value_t;

		enum slot_state { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD_INS };

		struct record { 
			key_t key; 
			value_t value;
			slot_state state;		
		} *table;

		std::size_t buckets;
		std::size_t records;
		std::size_t tombs;
		uint64_t table_head;
		int rebuild_window;
		
		int prime_index;
		double max_load_factor;

		uint64_t search_count;
		double miss_running_avg;

		uint64_t hash(key_t k) const;
		bool probe(key_t k, uint64_t *slot, optype operation,
		           bool* wrapped = NULL);
		uint64_t shift(uint64_t slot);
		int rebuild_seek(uint64_t x, uint64_t &end);
		uint64_t rebuild_shift(uint64_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline slot_state state(uint64_t k) const {
			return table[k].state;
		}
		inline key_t& key(uint64_t k) const {
			return table[k].key;
		}
		inline value_t& value(uint64_t k) const {
			return table[k].value;
		}

		inline void setkey(uint64_t k, key_t x)
			{ table[k].key = x; }
		inline void setvalue(uint64_t k, value_t v)
			{ table[k].value = v; }

		inline void setfull(uint64_t k) { table[k].state = FULL; }
		inline void setempty(uint64_t k) { table[k].state = EMPTY; }
		inline void settomb(uint64_t k) { table[k].state = TOMB; }

		inline bool full(uint64_t k) const {
			return state(k) == FULL;
		}
		inline bool empty(uint64_t k) const {
			return state(k) == EMPTY;
		}
		inline bool tomb(uint64_t k) const {
			return state(k) == TOMB;
		}

	public:
		enum result { SUCCESS, FAILURE, REBUILD, DUPLICATE, FULLTABLE };

		graveyard_aos(std::size_t b);
		~graveyard_aos();
		std::string table_type() { return "graveyard_aos"; }

		void resize(std::size_t);
		void set_max_load_factor(double f) { max_load_factor = f; }
		
		result insert(key_t key, value_t value, bool rebuilding = false);
		bool query(key_t key, value_t *value);
		result remove(key_t key);
		void rebuild();

		// Performance characteristics
		uint64_t total_misses;
		uint64_t inserts, queries, removes;
		uint64_t insert_misses, query_misses, remove_misses;
		uint64_t rebuild_inserts, rebuild_insert_misses;
		uint64_t insert_shifts;
		uint64_t failed_inserts, failed_queries, failed_removes, duplicates;
		uint64_t resizes;
		uint64_t longest_search;
		uint64_t rebuilds;
		int max_rebuild_queue;

		void reset_perf_counts();
		void report_testing_stats(std::ostream &os = std::cout,
				bool verbose = true);

		// return cluster length data, with clusters bounded either by
		// empty slots or by tombstones 
		void cluster_len(std::map<int, int>*) const;
		void shift_distance(std::map<int, int>*) const;

		int get_rebuild_window() { return rebuild_window; }
		double load_factor() { return (double)records/buckets; }
		double avg_misses() { return miss_running_avg; }
		std::size_t table_size() { return buckets; }
		std::size_t table_size_bytes() { return buckets*sizeof(record); }
		std::size_t num_records() { return records; }

		// debugging
		void dump();
		bool disable_rebuilds;
		bool check_ordering();
};
#endif
