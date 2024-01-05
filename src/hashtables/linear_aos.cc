#include <iostream>
#include <cassert>
#include "linear.h"
#include "primes.h"

using std::cerr, std::size_t;

template class linear_aos<>;

template <typename K, typename V>
linear_aos<K, V>::linear_aos(uint32_t b)
{
	prime_index = 0;
	while(b > primes[prime_index])
		prime_index++;

	table = new record_t[b];
	if (!table) cerr << "Couldn't allocate\n";

	for(uint32_t i=0; i<b; i++)
		table[i].state = EMPTY;

	buckets = b;
	records = 0;
	max_load_factor = 0.5;
	miss_running_avg = 0;
	search_count = 0;
	total_misses = 0;

	reset_perf_counts();
	reset_rebuild_window();
	rebuilds = 0;
	tombs = 0;
	disable_rebuilds = false;
}

template <typename K, typename V>
linear_aos<K, V>::~linear_aos()
{
	delete[] table;
}

template <typename K, typename V>
uint32_t
linear_aos<K, V>::hash(K k) const
{
	return (uint32_t)(((uint64_t)k * (uint64_t)buckets) >> 32);
}

template <typename K, typename V>
void
linear_aos<K, V>::resize(uint32_t b)
{
	uint32_t oldbuckets = buckets;
	record_t *oldtable = table;

	//cerr << "resize(): rehashing into " << b << " buckets\n";

	table = new record_t[b];
	if (!table) {
		cerr << "couldn't allocate for resize\n";
		exit(1);
	}
	for(uint32_t i=0; i<b; ++i)
		table[i].state = EMPTY;
	records = 0;
	buckets = b;
	tombs = 0;

	for(uint32_t i=0; i<oldbuckets; ++i)
		if (oldtable[i].state == FULL)
			insert(oldtable[i].key, oldtable[i].value, true);

	delete[] oldtable;
	resizes++;
}

template <typename K, typename V>
bool
linear_aos<K, V>::probe(K k, uint32_t *slot, optype operation)
{
	uint32_t probe = hash(k);
	uint32_t miss = 0;
	bool res = false;

	if (operation == INSERT || operation == REBUILD_INS) {
		while(true) {
			if (!full(probe)) {
				*slot = probe;
				res = true;
				break;
			} else if (key(probe) == k) {
				res = false;
				break;
			}
			if (++probe == buckets) probe = 0;
			++miss;
		}
	} else if (operation == QUERY || operation == REMOVE) {
		while(true) {
			if (full(probe) && key(probe) == k) {
				*slot = probe;
				res = true;
				break;
			} else if (empty(probe)) {
				res = false;
				break;
			}
			if (++probe == buckets) probe = 0;
			++miss;
		}
	}

	if (miss) update_misses(miss, operation);
	return res;
}

template <typename K, typename V>
linear_aos<K, V>::result
linear_aos<K, V>::insert(K k, V v, bool rebuilding)
{
	uint32_t slot;

	if (records>=buckets) {
		failed_inserts++;
		return FULLTABLE;
	}

	if(!probe(k, &slot, rebuilding ? REBUILD_INS : INSERT)) {
		failed_inserts++;
		return DUPLICATE;
	}

	if (tomb(slot)) --tombs;
	setkey(slot, k);
	setvalue(slot, v);
	setfull(slot);
	++records;

	rebuilding ? rebuild_inserts++ : inserts++;

	// automatic resizing
	if (load_factor() > max_load_factor) {
		cerr << "load factor " << max_load_factor << " exceeded\n";
		resize(primes[++prime_index]);
	}

	if (!rebuilding) {
		--rebuild_window;
		if (rebuild_window <= 0) return REBUILD;
	}

	return SUCCESS;
}

template <typename K, typename V>
bool
linear_aos<K, V>::query(K k, V *v)
{
	uint32_t slot;
	++queries;

	if (probe(k, &slot, QUERY)) {
		*v = value(slot);
		return true;
	} else {
		++failed_queries;
		return false;
	}
}

template <typename K, typename V>
linear_aos<K, V>::result
linear_aos<K, V>::remove(K k)
{
	uint32_t slot;
	++removes;
	if (probe(k, &slot, REMOVE)) {
		table[slot].state = TOMB;
		++tombs;
		--records;
		return result::SUCCESS;
	} else {
		++failed_removes;
		return result::FAILURE;
	}
}

template <typename K, typename V>
void
linear_aos<K, V>::reset_rebuild_window()
{
	rebuild_window = buckets/2 * (1.0 - load_factor());
}

template <typename K, typename V>
void
linear_aos<K, V>::rebuild()
{
	resize(buckets);
	reset_rebuild_window();
}

template <typename K, typename V>
void
linear_aos<K, V>::update_misses(uint64_t misses, enum optype op)
{
	int n = ++search_count;
	total_misses += misses;
	switch(op) {
	case INSERT: insert_misses += misses; break;
	case QUERY: query_misses += misses; break;
	case REMOVE: remove_misses += misses; break;
	case REBUILD_INS: rebuild_insert_misses += misses; break;
	}
	if (misses > longest_search) longest_search = misses;
	miss_running_avg = miss_running_avg * (double)(n-1)/n
	                   + (double)misses/n;
}

template <typename K, typename V>
void
linear_aos<K, V>::reset_perf_counts()
{
	inserts = queries = removes = rebuild_inserts = 0;
	insert_misses = query_misses = remove_misses = 0;
	rebuild_insert_misses = 0;
	failed_inserts = failed_removes = failed_queries = duplicates = 0;
	insert_shifts = longest_search = 0;
	total_misses = 0;
	miss_running_avg = 0;
	search_count = 0;
	resizes = 0;
}

template <typename K, typename V>
void
linear_aos<K, V>::report_testing_stats(std::ostream &os, bool verbose)
{
	if (verbose) {
		os << "Misses\n";
		os << "Insert: " << insert_misses;
		if (inserts)
			os << ", " << (double)insert_misses/inserts
			   << " miss/insert\n";

		os << "Query: " << query_misses;
		if (queries)
			os << ", " << (double)query_misses/queries
			   << " miss/query\n";

		os << "Remove: " << remove_misses;
		if (removes)
			os << ", " << (double)remove_misses/removes
			   << " miss/remove\n";

		os << "Total: " << total_misses
		   << ", " << (double)total_misses/(inserts+queries+removes)
		   << " miss/op\n";

		os << "Fails\nInserts: " << failed_inserts
		   << " (" << duplicates << " dup) / " << inserts
		   << ", Queries: " << failed_queries << " / " << queries
		   << ", Removes: " << failed_removes << " / " << removes
		   << "\n";
	} else {
		os << insert_misses << ","
		   << (inserts ? (double)insert_misses/inserts : 0) << ","
		   << query_misses << ","
		   << (queries ? (double)query_misses/queries : 0) << ","
		   << remove_misses << ","
		   << (removes ? (double)remove_misses/removes : 0) << ","
		   << total_misses << ","
		   << (double)total_misses/(inserts+queries+removes)
		   << "\n";
	}
}

// fill in a histogram of cluster lengths (tombstones count as boundaries)
template<typename K,typename V>
void
linear_aos<K,V>::cluster_len(std::map<int,int> *clust) const
{
	uint32_t first_nonfull, last_nonfull, p=0;

	while (full(p)) ++p;
	first_nonfull = last_nonfull = p;

	while(p < buckets) {
		if (!full(p)) {
			int dist = p - last_nonfull;
			if (dist > 1) (*clust)[dist-1]++;
			last_nonfull = p;
		}
		p++;
	}

	(*clust)[buckets-last_nonfull+first_nonfull-1]++;
}

// fill in a histogram of shift lengths
// i.e. the distance from a key's slot and the hash of that key
template<typename K, typename V>
void
linear_aos<K,V>::search_distance(std::map<int,int> *disp) const
{
	for(uint32_t p = 0; p < buckets; ++p)
		if (full(p)) {
			uint32_t h = hash(key(p));
			int d = (p > h ? p - h : buckets - h + p);
			(*disp)[d]++;
		}
}

template<typename K, typename V>
void
linear_aos<K, V>::dump()
{
	for(size_t i=0; i<buckets; i++) {
		if ((i!=0) && (i%10 == 0)) std::cout << "\n";
		std::cout.width(4);
		std::cout << i << ':';

		if (tomb(i)) std::cout << "\e[1;31m";
		std::cout << " [";

		if(full(i)) {
			std::cout.width(4);
			std::cout << hash(key(i)) << "]";
			std::cout.width(4);
			std::cout << key(i);
		} else if (empty(i)) {
			std::cout << "    ]    ";
		} else {
			std::cout.width(4);
			std::cout << "____]";
			std::cout << "____";
		}
		if (tomb(i)) std::cout << "\e[0m";
	}
}
