#ifndef ORDERED_H
#define ORDERED_H

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <map>

template <typename K = uint32_t,
          typename V = uint32_t>
class ordered_aos {
	private:
		enum slot_state { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD_INS };

		struct record {
			K key;
			V value;
			slot_state state;
		} *table;

		uint32_t buckets;
		uint32_t records;
		uint32_t tombs;
		uint32_t table_head;
		int rebuild_window;
		
		int prime_index;
		double max_load_factor;

		uint64_t search_count;
		double miss_running_avg;

		uint32_t hash(K k) const;
		bool probe(K k, uint32_t *slot, optype operation,
		           bool* wrapped = NULL);
		uint32_t shift(uint32_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline slot_state state(uint32_t k) const {
			return table[k].state;
		}
		inline K& key(uint32_t k) const {
			return table[k].key;
		}
		inline V& value(uint32_t k) const {
			return table[k].value;
		}

		inline void setkey(uint32_t k, K x)
			{ table[k].key = x; }
		inline void setvalue(uint32_t k, V v)
			{ table[k].value = v; }

		inline void setfull(uint32_t k) { table[k].state = FULL; }
		inline void setempty(uint32_t k) { table[k].state = EMPTY; }
		inline void settomb(uint32_t k) { table[k].state = TOMB; }

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

		ordered_aos(uint32_t b);
		~ordered_aos();
		std::string table_type() const { return "ordered_aos"; }

		void resize(uint32_t);
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

template <typename K = uint32_t,
          typename V = uint32_t>
class ordered_soa {
	private:
		enum slot_state { FULL, EMPTY, TOMB };
		enum optype { INSERT, QUERY, REMOVE, REBUILD_INS };

		struct record_t {
			K key;
			V value;
			slot_state state;
		};

		struct table_t {
			K *key;
			V *value;
			slot_state *state;
		} table;

		std::size_t buckets;
		std::size_t records;
		std::size_t tombs;
		uint32_t table_head;
		int rebuild_window;
		
		int prime_index;
		double max_load_factor;

		uint64_t search_count;
		double miss_running_avg;

		uint32_t hash(K k) const;
		bool probe(K k, uint32_t *slot, optype operation,
		           bool* wrapped = NULL);
		uint32_t shift(uint32_t slot);

		void reset_rebuild_window();
		void update_misses(uint64_t misses, enum optype op);

		inline void slotmove(uint32_t destidx, uint32_t srcidx,
		                     size_t count);

		inline slot_state state(uint32_t k) const {
			return table.state[k];
		}
		inline K& key(uint32_t k) const {
			return table.key[k];
		}
		inline V& value(uint32_t k) const {
			return table.value[k];
		}

		inline void setkey(uint32_t k, K x)
			{ table.key[k] = x; }
		inline void setvalue(uint32_t k, V v)
			{ table.value[k] = v; }

		inline void setfull(uint32_t k) { table.state[k] = FULL; }
		inline void setempty(uint32_t k) { table.state[k] = EMPTY; }
		inline void settomb(uint32_t k) { table.state[k] = TOMB; }

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

		ordered_soa(uint32_t b);
		~ordered_soa();
		std::string table_type() const { return "ordered_soa"; }

		void resize(uint32_t);
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
		void search_distance(std::map<int, int>*) const;

		int get_rebuild_window() const { return rebuild_window; }
		double load_factor() const { return (double)records/buckets; }
		double avg_misses() const { return miss_running_avg; }
		uint32_t table_size() const { return buckets; }
		uint64_t table_size_bytes() const {
			return buckets*sizeof(record_t);
		}
		uint32_t num_records() const { return records; }

		// debugging
		void dump();
		bool disable_rebuilds;
		bool check_ordering();
};
#endif
