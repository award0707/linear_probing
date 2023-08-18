#include <iostream>
#include <cassert>
#include "linear.h"
#include "primes.h"

using std::cerr, std::size_t;

linear_aos::linear_aos(uint64_t b)
{
	prime_index = 0;
	while(b > primes[prime_index]) 
		prime_index++;
	
	table = new record[b];
	if (!table) cerr << "Couldn't allocate\n";
	
	for(uint64_t i=0; i<b; i++) 
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

linear_aos::~linear_aos()
{
	delete[] table;
}

uint64_t linear_aos::hash(int k)
{
	int r = k % buckets;
	return (r<0) ? r+buckets : r;
}

void linear_aos::resize(uint64_t b)
{
	uint64_t oldbuckets = buckets;
	record *oldtable = table;

	cerr << "resize(): rehashing into " << b << " buckets\n";
	
	table = new record[b];
	if (!table) cerr << "couldn't allocate for resize\n"; 
	for(uint64_t i=0; i<b; ++i) 
		table[i].state = EMPTY;
	records = 0;
	buckets = b;

	for(uint64_t i=0; i<oldbuckets; ++i) {
		if (oldtable[i].state == FULL)
			insert(oldtable[i].key, oldtable[i].value, true);
	}

	delete[] oldtable;
	resizes++;	
}

bool linear_aos::probe(int k, uint64_t *slot, optype operation)
{
	uint64_t probe = hash(k);
	uint32_t miss = 0;
	bool res = false;

	if (operation == INSERT || operation == REBUILD) {
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

linear_aos::insert_result linear_aos::insert(int k, int v, bool rebuilding) 
{
	uint64_t slot;

	if (records>=buckets) {
		failed_inserts++;
		return FAIL_FULL;
	}

	if(!probe(k, &slot, rebuilding ? REBUILD : INSERT)) {
		failed_inserts++;
		return FAIL_DUP;
	}
	if (tomb(slot)) --tombs;
	table[slot] = {k, v, FULL};
	++records;
	
	rebuilding ? rebuild_inserts++ : inserts++;

	// automatic resizing
	if (load_factor() > max_load_factor) { 
		cerr << "load factor " << max_load_factor << " exceeded\n";
		resize(primes[++prime_index]);
		tombs = 0;
	}

	if (!rebuilding) { 
		--rebuild_window;
		if (rebuild_window <= 0 && !disable_rebuilds) return NEEDS_REBUILD;
	}

	return SUCCESS;
}

bool 
linear_aos::query(int k, int *v) 
{
	uint64_t slot;
	++queries;

	if (probe(k, &slot, QUERY)) {
		*v = value(slot);
		return true;
	} else {
		++failed_queries;
		return false;
	}
}

bool
linear_aos::remove(int k)
{
	uint64_t slot;
	++removes;	
	if (probe(k, &slot, REMOVE)) {
		table[slot].state = TOMB;
		++tombs;
		--records;
		return true;
	} else {
		++failed_removes;
		return false;
	}
}


void
linear_aos::reset_rebuild_window()
{
	rebuild_window = buckets/2 * (1.0 - load_factor());
}

void
linear_aos::rebuild()
{
	resize(buckets);
}

void linear_aos::update_misses(uint64_t misses, enum optype op)
{
	int n = ++search_count;
	total_misses += misses;
	switch(op) {
		case INSERT:
			insert_misses += misses; break;
		case QUERY:
			query_misses += misses; break;
		case REMOVE:
			remove_misses += misses; break;
		case REBUILD:
			rebuild_insert_misses += misses; break;
	}
	if (misses > longest_search) longest_search = misses;
	miss_running_avg = miss_running_avg * (double)(n-1)/n + (double)misses/n;
}

void linear_aos::reset_perf_counts()
{
	inserts = queries = removes = rebuild_inserts = 0;
	insert_misses = query_misses = remove_misses = rebuild_insert_misses = 0;
	failed_inserts = failed_removes = failed_queries = duplicates = 0;
	longest_search = 0;
	total_misses = 0;
	miss_running_avg = 0;
	search_count = 0;
	resizes = 0;
}

void linear_aos::report_testing_stats(std::ostream &os, bool verbose)
{
	if (verbose) {
		os << "Misses\n";
		os << "Insert: " << insert_misses;
		if (inserts)
			os << ", " << (double)insert_misses/inserts << " miss/insert";
		os << "\n";
		
		os << "Query: " << query_misses;
		if (queries)
			os << ", " << (double)query_misses/queries << " miss/query";
		os << "\n";
		
		os << "Remove: " << remove_misses;
		if (removes)
				os << ", " << (double)remove_misses/removes << " miss/remove";
		os << "\n";

		os << "Total: " << total_misses
			<< ", " << (double)total_misses/(inserts+queries+removes)
			<< " miss/op\n";

		os << "Fails\nInserts: " << failed_inserts
				<< " (" << duplicates << " dup) / " << inserts
				<< ", Queries: " << failed_queries << " / " << queries 
				<< ", Removes: " << failed_removes << " / " << removes << "\n";	
	} else {
		os << insert_misses << ","
			<< (inserts ? (double)insert_misses/inserts : 0) << ","
			<< query_misses << ","
			<< (queries ? (double)query_misses/queries : 0) << ","
			<< remove_misses << ","
			<< (removes ? (double)remove_misses/removes : 0) << ","
			<< total_misses << ","
			<< (double)total_misses/(inserts+queries+removes) << "\n";
	}
}

void linear_aos::cluster_len(std::vector<int> &empty_clust,
					   std::vector<int> &tomb_clust)
{
	// count two sorts of cluster:
	// one bounded by tombstones (as in a query)
	// and one bounded by empty slots (so can contain tombstones)
	int empty = 0, tomb = 0; 
	for (std::size_t i=0; i<buckets; ++i) {
		switch(table[i].state) {
		case FULL:
			++empty;
			++tomb;
			break;
		case EMPTY:
			if (empty) empty_clust.push_back(empty);
			if (tomb) tomb_clust.push_back(tomb);
			empty = tomb = 0;
			break;
		case TOMB:
			++empty;
			if (tomb) tomb_clust.push_back(tomb);
			tomb = 0;
			break;
		}
	}
	if (empty) empty_clust.push_back(empty);
	if (tomb) tomb_clust.push_back(tomb);
}
