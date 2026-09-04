#ifndef ORDERED_H
#define ORDERED_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>

class ordered {
	private:
		enum slot_state : uint8_t { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD_INS };

		struct record {
			uint32_t key;
			uint32_t value;
		} *table;
		enum slot_state *states;

		uint32_t buckets;
		uint32_t records;
		uint32_t tombs;
		uint32_t table_head;
		int rebuild_window;
		
		int prime_index;
		double max_load_factor;

		uint64_t search_count;
		double miss_running_avg;

		uint32_t hash(uint32_t k) const;
		bool probe(uint32_t k, uint32_t *slot, optype operation,
		           bool* wrapped = NULL);
		uint32_t shift(uint32_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline slot_state state(uint32_t k) const {
			return states[k];
		}
		inline uint32_t& key(uint32_t k) const {
			return table[k].key;
		}
		inline uint32_t& value(uint32_t k) const {
			return table[k].value;
		}

		inline void setkey(uint32_t k, uint32_t x)
			{ table[k].key = x; }
		inline void setvalue(uint32_t k, uint32_t v)
			{ table[k].value = v; }

		inline void setfull(uint32_t k) { states[k] = FULL; }
		inline void setempty(uint32_t k) { states[k] = EMPTY; }
		inline void settomb(uint32_t k) { states[k] = TOMB; }

		inline bool full(uint32_t k) const {
			return state(k) == FULL;
		}
		inline bool empty(uint32_t k) const {
			return state(k) == EMPTY;
		}
		inline bool tomb(uint32_t k) const {
			return state(k) == TOMB;
		}

	public:
		enum result { SUCCESS, FAILURE, REBUILD, DUPLICATE, FULLTABLE };

		ordered(uint32_t b);
		~ordered();
		std::string table_type() const { return "ordered_aos"; }

		void resize(uint32_t);
		void set_max_load_factor(double f) { max_load_factor = f; }
		
		result insert(uint32_t key, uint32_t value, bool rebuilding = false);
		bool query(uint32_t key, uint32_t *value);
		result remove(uint32_t key);
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
		void search_distance(std::map<int, int>*) const;

		int get_rebuild_window() const { return rebuild_window; }
		double load_factor() const { return (double)records/buckets; }
		double avg_misses() const { return miss_running_avg; }
		uint32_t table_size() const { return buckets; }
		uint64_t table_size_bytes() const {
			return buckets*sizeof(record);
		}
		uint32_t num_records() const { return records; }

		// debugging
		void dump();
		bool disable_rebuilds;
		bool check_ordering();
};

#endif

