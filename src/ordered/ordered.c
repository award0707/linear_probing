#include <iostream>
#include <cassert>
#include <cstring>
#include "ordered.h"
#include "primes.h"

ordered_aos::ordered_aos(uint64_t b)
{
	prime_index = 0;
	while(b > primes[prime_index]) 
		prime_index++;
	
	table = new record[b];
	if (!table) std::cerr << "Couldn't allocate\n";
	
	for(uint64_t i=0; i<b; i++) 
		table[i].state = EMPTY;

	max_load_factor = 0.5;
	miss_running_avg = 0;
	search_count = 0;
	total_misses = 0;

	buckets = b;
	records = 0;
	rebuilds = 0;
	tombs = 0;
	table_head = 0;
	disable_rebuilds = false;

	reset_perf_counts();
	reset_rebuild_window();
}	

ordered_aos::~ordered_aos()
{
	delete[] table;
}

uint64_t ordered_aos::hash(int64_t k)
{
	int64_t r = k % buckets;
	return (r<0) ? r+buckets : r;
}

void ordered_aos::resize(uint64_t b)
{
	uint64_t oldbuckets = buckets;
	record *oldtable = table;

	std::cerr << "resize(): rehashing into " << b << " buckets\n";
	
	table = new record[b];
	if (!table) std::cerr << "couldn't allocate for resize\n"; 
	for(uint64_t i=0; i<b; ++i) 
		table[i].state = EMPTY;
	records = 0;
	tombs = 0;
	buckets = b;

	for(uint64_t i=0; i<oldbuckets; ++i) {
		if (oldtable[i].state == FULL)
			insert(oldtable[i].key, oldtable[i].value, true);
	}

	delete[] oldtable;
	resizes++;	
}

bool ordered_aos::probe(int k, uint64_t *slot, optype operation, bool* wrapped)
{
	const uint64_t h = hash(k);
	uint64_t miss = 1;
	bool res = false;
	uint64_t s = std::max(h, table_head);

	switch(operation) {
	case INSERT:
	case REBUILD_INS:
		res = true;
		while(1) {
			if (full(s) && key(s) == k) {	// duplicate key
				res = false;
				break;
			}
			if (empty(s) || (full(s) && hash(key(s)) > h)) break;
			if (++s == buckets) {
				s = 0;
				*wrapped = true;
			}
			++miss;
			if (s == table_head) break;
		}
		break;
	case QUERY:
	case REMOVE:
		res = false;
		while(1) {
			if (full(s) && key(s) == k) {
				res = true;
				break;
			}
			if (empty(s) || (full(s) && hash(key(s)) > h)) break;
			if (++s == buckets) s = 0;
			++miss;
			if (s == table_head) break;
		}
		break;
	}

	if (miss) update_misses(miss, operation);
	*slot = s;
	return res;
}

// find the end of the cluster, then slide records 1 to the right
uint64_t ordered_aos::shift(uint64_t start)
{
	using std::memmove;
	const uint64_t last = buckets-1;
	uint64_t end = start;
	
	do
		if (++end > last) end = 0;
	while (full(end));

	if (tomb(end)) --tombs; // if we made use of a tombstone

	if (end < start) {
		memmove(&table[1], &table[0], sizeof(record)*end);
		table[0] = table[last];
		memmove(&table[start+1],
			&table[start], sizeof(record)*(last-start));
	} else
		memmove(&table[start+1],
			&table[start], sizeof(record)*(end-start));

	return end;
}

ordered_aos::result ordered_aos::insert(int k, int v, bool rebuilding) 
{
	uint64_t slot;
	bool wrapped=false;

	if (records>=buckets) {
		failed_inserts++;
		return result::FULLTABLE;
	}

	optype ins_type = rebuilding ? optype::REBUILD_INS : optype::INSERT;
	if (!probe(k, &slot, ins_type, &wrapped)) {
		++failed_inserts;
		++duplicates;
		return result::DUPLICATE;
	}

	if (!empty(slot)) {
		uint64_t end = shift(slot);
		if (((end < slot) || wrapped) && end >= table_head) ++table_head;
		if (!rebuilding) {
			if (end >= slot)
				insert_shifts += end - slot;
			else
				insert_shifts += buckets - slot + end;
		}
	} else if (wrapped && slot == table_head)
		table_head++;
	
	setkey(slot, k);
	setvalue(slot, v);
	setfull(slot);

	++records;
	rebuilding ? rebuild_inserts++ : inserts++;

	// automatic resizing
	if (load_factor() > max_load_factor) { 
		std::cerr << "load factor " << max_load_factor << " exceeded\n";
		resize(primes[++prime_index]);
	}

	if (!rebuilding) { 
		--rebuild_window;
		if (rebuild_window <= 0) return result::REBUILD;
	}

	return result::SUCCESS;
}

bool 
ordered_aos::query(int k, int *v) 
{
	uint64_t slot;
	++queries;

	if (probe(k, &slot, QUERY)) {
		*v = value(slot);
		return true;
	}
	
	++failed_queries;
	return false;
}

bool
ordered_aos::remove(int k)
{
	uint64_t slot;
	++removes;	

	if (probe(k, &slot, REMOVE)) {
		settomb(slot);
		++tombs;
		--records;
		return true;
	}

	++failed_removes;
	return false;
}

void
ordered_aos::reset_rebuild_window()
{
	rebuild_window = buckets/2 * (1.0 - load_factor());
}

void
ordered_aos::rebuild()
{
	std::vector<record> overflow;

	// temporarily save the table overflow 
	for(uint64_t p = 0; p < table_head; ++p) {
		if (full(p)) {
			overflow.push_back(table[p]);
			--records;
		}
		table[p].state = EMPTY;
	}
	table_head = 0;

	// slide elements down
	for(uint64_t p = 0, q = 0; p < buckets; ++p, ++q) {
		if (!full(p)) {
			while (!full(q) && q < buckets) {
				table[q].state = EMPTY;
				++q;
			}
			if (q == buckets) break;

			uint64_t h = hash(key(q));
			if (p < h) p = h;
			if (p != q) {
				table[p] = table[q];
				table[q].state = EMPTY;
			}
		}
	}

	// reinsert the table overflow.
	for (record r : overflow) insert(r.key, r.value, true);

	++rebuilds;
	reset_rebuild_window();	
}

void ordered_aos::update_misses(uint64_t misses, enum optype op)
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

	miss_running_avg =
		miss_running_avg * (double)(n-1)/n + (double)misses/n;
}

void ordered_aos::reset_perf_counts()
{
	inserts = queries = removes = rebuild_inserts = 0;
	insert_misses = query_misses = remove_misses = 0;
	rebuild_insert_misses = 0;
	insert_shifts = 0;
	failed_inserts = failed_removes = failed_queries = duplicates = 0;
	longest_search = 0;
	total_misses = 0;
	miss_running_avg = 0;
	search_count = 0;
	resizes = 0;
}

void ordered_aos::report_testing_stats(std::ostream &os, bool verbose)
{
	if (verbose) {
		os << "Misses\n";
		os << "Insert: " << insert_misses;
		if (inserts)
			os << ", " << (double)insert_misses/inserts
				<< " miss/insert";
		os << "\n";
		
		os << "Query: " << query_misses;
		if (queries)
			os << ", " << (double)query_misses/queries
				<< " miss/query";
		os << "\n";
		
		os << "Remove: " << remove_misses;
		if (removes)
			os << ", " << (double)remove_misses/removes
					<< " miss/remove";
		os << "\n";

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
void ordered_aos::cluster_len(std::map<int,int> *clust) const
{
	uint64_t last_empty, last_tomb; 
	last_empty = last_tomb = table_head;
	for(uint64_t p = table_head; p < buckets; ++p) {
		if (!full(p)) {
			int dist = std::min(p - last_empty, p - last_tomb);
			if (dist > 1) (*clust)[dist-1]++;
			if (empty(p)) last_empty = p;
			if (tomb(p)) last_tomb = p;
		}
	}

	// keep counting once we wrap the table
	for(uint64_t p = 0; p < table_head; ++p) {
		if (!full(p)) {
			// detect if the cluster wrapped
			int x = last_empty >= table_head ?
				(buckets - last_empty + p) : (p - last_empty);
			int y = last_tomb >= table_head ?
				(buckets - last_tomb + p) : (p - last_tomb);
			int dist = std::min(x, y);
			if (dist > 1) (*clust)[dist-1]++;
			if (empty(p)) last_empty = p;
			if (tomb(p)) last_tomb = p;
		}
	}
}

// fill in a histogram of shift lengths
// i.e. the distance from a key's slot and the hash of that key
void ordered_aos::shift_distance(std::map<int,int> *disp) const
{
	for(uint64_t p = 0; p < buckets; ++p) {
		if (full(p)) {
			uint64_t h = hash(key(p));
			int d = (p >= table_head ? p - h : buckets - h + p);
			assert(d >= 0); // invariant broken
			(*disp)[d]++;
		}
	 }
}

// ensure keys are monotonically increasing
bool ordered_aos::check_ordering()
{
	uint64_t p = table_head, q;
	bool wrapped = false;

	while (table[p].state != FULL) ++p;
	q = p;
	while(1) {
		while (table[++q].state != FULL)
			if (q == buckets) { 
				q = 0;
				wrapped = true;
			}

		if (wrapped && q >= table_head) break;

		if (hash(key(p)) > hash(key(q))) {
			std::cerr << "Ordering violated at slot " << q << "\n";
			return false;
		}

		p = q;
	}

	return true;
}
