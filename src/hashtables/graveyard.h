#ifndef GRAVEYARD_H
#define GRAVEYARD_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>

class graveyard {
	private:
		enum slot_state : char { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD_INS };

		struct record {
			uint32_t key;
			uint32_t value;
		} *table;
		enum slot_state *states;

		uint32_t slots, keycount, tombcount;
		uint32_t blocksize, blockcount;
		int rebuild_window;
		uint32_t table_head;

		int prime_index;
		double max_load_factor;

		uint32_t hash(uint32_t k) const;
		bool probe(uint32_t k, uint32_t *slot);
		bool insert_probe(uint32_t k, uint32_t *slot, bool* wrapped);
		uint32_t shift(uint32_t slot);
		inline void slotmove(uint32_t destidx, uint32_t srcidx,
		     size_t count);

		// used during the table rebuild phase (to protect tombs)
		result rb_insert(uint32_t k, uint32_t v);
		int rb_seek(uint32_t x, uint32_t &end);
		uint32_t rb_shift(uint32_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline slot_state state(uint32_t k) const { return states[k]; }
		inline uint32_t key(uint32_t k) const { return table[k].key; }
		inline uint32_t value(uint32_t k) const { return table[k].value; }

		inline void setkey(uint32_t k, uint32_t x) { table[k].key = x; }
		inline void setvalue(uint32_t k, uint32_t v) { table[k].value = v; }

		inline void setfull(uint32_t k) { states[k] = FULL; }
		inline void setempty(uint32_t k) { states[k] = EMPTY; }
		inline void settomb(uint32_t k) { states[k] = TOMB; }

		inline bool full(uint32_t k) const { return state(k) == FULL; }
		inline bool empty(uint32_t k) const { return state(k) == EMPTY; }
		inline bool tomb(uint32_t k) const { return state(k) == TOMB; }

	public:
		enum result { SUCCESS, FAILURE, REBUILD, DUPLICATE, FULLTABLE };

		graveyard(uint32_t n, uint32_t b);
		~graveyard();
		std::string table_type() const { return "graveyard_aos"; }

		void resize(uint32_t);
		void set_max_load_factor(double f) { max_load_factor = f; }

		result insert(uint32_t key, uint32_t value);
		bool query(uint32_t key, uint32_t *value);
		result remove(uint32_t key);
		void rebuild();

		// Statistics collection, etc.
		uint64_t search_count;
		double miss_running_avg;

		uint64_t total_misses;
		uint64_t inserts, queries, removes, duplicates;
		uint64_t insert_misses, query_misses, remove_misses;
		uint64_t rebuild_inserts, rebuild_insert_misses;
		uint64_t failed_inserts, failed_queries, failed_removes;
		uint64_t resizes, rebuilds;
		uint64_t insert_shifts, longest_search;
		int max_rebuild_queue;

		void reset_perf_counts();
		void report_testing_stats(std::ostream &os = std::cout,
		                          bool verbose = true);

		// return cluster length data, with clusters bounded either by
		// empty slots or by tombstones
		void cluster_len(std::map<int, int>*) const;
		void search_distance(std::map<int, int>*) const;

		int get_rebuild_window() const { return rebuild_window; }
		double load_factor() const { return (double)keycount/slots; }
		double avg_misses() const { return miss_running_avg; }
		uint32_t table_size() const { return slots; }
		std::size_t table_size_bytes() const { return slots*sizeof(record); }
		std::size_t rec_width() const { return sizeof(table[0]); }
		std::size_t key_width() const { return sizeof(table[0].key); }
		std::size_t value_width() const { return sizeof(table[0].value); }
		std::size_t state_width() const { return sizeof(states[0]); }
		uint32_t num_records() const { return keycount; }

		// debugging
		void dump();
		bool disable_rebuilds;
		bool check_ordering();
		void debug_key_search(uint32_t k);
};

#endif

