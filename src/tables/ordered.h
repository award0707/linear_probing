#ifndef ORDERED_H
#define ORDERED_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>

template <typename K = int, typename V = int>
class ordered_aos {
	private:
		enum slot_state { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD_INS };

		struct record {
			K key;
			V value;
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

		uint64_t hash(K k) const;
		bool probe(K k, uint64_t *slot, optype operation,
		           bool* wrapped = NULL);
		uint64_t shift(uint64_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline slot_state state(uint64_t k) const {
			return table[k].state;
		}
		inline K& key(uint64_t k) const {
			return table[k].key;
		}
		inline V& value(uint64_t k) const {
			return table[k].value;
		}

		inline void setkey(uint64_t k, K x)
			{ table[k].key = x; }
		inline void setvalue(uint64_t k, V v)
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

		ordered_aos(uint64_t b);
		~ordered_aos();
		std::string table_type() { return "ordered_aos"; }

		void resize(uint64_t);
		void set_max_load_factor(double f) { max_load_factor = f; }
		
		result insert(K key, V value, bool rebuilding = false);
		bool query(K key, V *value);
		result remove(K key);
		void rebuild();

		// Performance characteristics
		uint64_t total_misses;
		uint64_t inserts, queries, removes;
		uint64_t insert_misses, query_misses, remove_misses;
		uint64_t insert_shifts, longest_search;
		uint64_t rebuild_inserts, rebuild_insert_misses;
		uint64_t failed_inserts, failed_queries, failed_removes;
		uint64_t duplicates;
		uint64_t resizes, rebuilds;

		void reset_perf_counts();
		void report_testing_stats(std::ostream &os = std::cout,
		                          bool verbose = true);

		void cluster_len(std::map<int, int>*) const;
		void shift_distance(std::map<int, int>*) const;

		int get_rebuild_window() const { return rebuild_window; }
		double load_factor() const { return (double)records/buckets; }
		double avg_misses() const { return miss_running_avg; }
		uint64_t table_size() const { return buckets; }
		uint64_t table_size_bytes() const {
			return buckets*sizeof(record);
		}
		uint64_t num_records() const { return records; }

		// debugging
		void dump();
		bool disable_rebuilds;
		bool check_ordering();
};
#endif
