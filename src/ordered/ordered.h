#ifndef ORDERED_H
#define ORDERED_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>


class ordered_aos {
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

		uint64_t hash(int64_t k);
		bool probe(int k, uint64_t *slot, optype operation,
		           bool* wrapped = NULL);
		uint64_t shift(uint64_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline void setkey(uint64_t k, key_t x)
			{ table[k].key = x; }
		inline void setvalue(uint64_t k, value_t v)
			{ table[k].value = v; }

		inline slot_state state(uint64_t k) const {
			return table[k].state;
		}
		inline key_t& key(uint64_t k) const {
			return table[k].key;
		}
		inline value_t& value(uint64_t k) const {
			return table[k].value;
		}

		inline void setfull(uint64_t k) { table[k].state = FULL; }
		inline void setempty(uint64_t k) { table[k].state = EMPTY; }
		inline void settomb(uint64_t k) {table[k].state = TOMB; }

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
		
		result insert(int key, int value, bool rebuilding = false);
		bool query(int key, int *value);
		result remove(int key);
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
		friend void dump(ordered_aos &);
		bool disable_rebuilds;
		bool check_ordering();
};
#endif
