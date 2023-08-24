#include <iostream>
#include <cassert>
#include <cstring>
#include "graveyard.h"
#include "primes.h"
#include <boost/circular_buffer.hpp>

using std::cerr, std::size_t;

template class graveyard_aos<>;

template<typename K, typename V>
graveyard_aos<K, V>::graveyard_aos(uint32_t b)
{
	prime_index = 0;
	while(b > primes[prime_index]) 
		prime_index++;
	
	table = new record_t[b];
	if (!table) cerr << "Couldn't allocate\n";
	
	for(uint32_t i=0; i<b; i++) 
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

template<typename K, typename V>
graveyard_aos<K, V>::~graveyard_aos()
{
	delete[] table;
}

template<typename K, typename V>
uint32_t
graveyard_aos<K, V>::hash(uint32_t k) const
{
	return (uint32_t)(((uint64_t)k * (uint64_t)buckets) >> 32);
}

template<typename K, typename V>
void
graveyard_aos<K, V>::resize(uint32_t b)
{
	uint32_t oldbuckets = buckets;
	record_t *oldtable = table;

	cerr << "resize(): rehashing into " << b << " buckets\n";
	
	table = new record_t[b];
	if (!table) cerr << "couldn't allocate for resize\n"; 
	for(uint32_t i=0; i<b; ++i) 
		table[i].state = EMPTY;
	records = 0;
	tombs = 0;
	buckets = b;

	for(uint32_t i=0; i<oldbuckets; ++i) {
		if (oldtable[i].state == FULL)
			insert(oldtable[i].key, oldtable[i].value, true);
	}

	delete[] oldtable;
	resizes++;	
}

template<typename K, typename V>
bool
graveyard_aos<K, V>::probe(K k, uint32_t *slot, optype operation, bool* wrapped)
{
	const uint32_t h = hash(k);
	uint64_t miss = 0;
	bool res = false;
	uint32_t s = std::max(h, table_head);

	switch(operation) {
	case INSERT:
	case REBUILD_INS:
		res = true;
		while(1) {
			if (full(s) && key(s) == k) {	// duplicate key
				res = false;
				break;
			}
			if (empty(s) || (full(s) && hash(key(s)) >= h)) break;
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

// find the end of the cluster, then slide records 1 to the right as a block

template<typename K, typename V>
uint32_t
graveyard_aos<K, V>::shift(uint32_t start)
{
	using std::memmove;
	const uint32_t last = buckets-1;
	uint32_t end = start;
	
	do
		if (++end > last) end = 0;
	while (full(end));

	if (tomb(end)) --tombs; // made use of a tombstone

	if (end < start) {
		memmove(&table[1], &table[0], sizeof(record_t)*end);
		table[0] = table[last];
		memmove(&table[start+1],
			&table[start], sizeof(record_t)*(last-start));
	} else
		memmove(&table[start+1],
			&table[start], sizeof(record_t)*(end-start));

	return end;
}


template<typename K, typename V>
int
graveyard_aos<K, V>::rebuild_seek(uint32_t x, uint32_t &end)
{
	const uint32_t last = buckets-1;
	while(1) {
		if (x > last) {
			end = last;
			return 2;       // shift into the end of the table
		} else if (empty(x)) {
			end = x;
			return 0;       // shift into an empty slot
		} else if (tomb(x)) {
			end = x-1;
			return 1;       // shift into a tombstone
		}
		++x;
	}
}

template<typename K, typename V>
uint32_t
graveyard_aos<K, V>::rebuild_shift(uint32_t start)
{
	record_t lastscratch, scratch;
	bool valid = false;
	uint32_t end;

	while(1) {
		int res = rebuild_seek(start, end);

		scratch = table[end];
		std::memmove(&table[start+1],&table[start],
		             (end-start)*sizeof table[start]);
		if (valid) table[start] = lastscratch;
		lastscratch = scratch;
		valid = true;

		if (res == 0) return end;
		start = end + 2;
		if (res == 2 || (res == 1 && start > buckets-1)) {
			end = rebuild_shift(0);
			table[0] = lastscratch;
			return end;
		}
	}
}

template<typename K, typename V>
graveyard_aos<K,V>::result
graveyard_aos<K,V>::insert(K k, V v, bool rebuilding)
{
	uint32_t slot;
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
		uint32_t end;
		end = (!rebuilding) ? shift(slot) : rebuild_shift(slot);
		if (((end < slot) || wrapped) && end >= table_head)
			++table_head;
		if (!rebuilding) {
			if (end >= slot)
				insert_shifts += end - slot;
			else
				insert_shifts += (buckets - slot + end);
		}
	} else if (wrapped && slot == table_head)
		table_head++;
	
	if (rebuilding && tomb(table_head)) ++table_head;
	setkey(slot, k);
	setvalue(slot, v);
	setfull(slot);
	
	// more stat recording
	++records;
	if (rebuilding) rebuild_inserts++; else inserts++;

	// automatic resizing
	if (load_factor() > max_load_factor) { 
		cerr << "load factor " << max_load_factor << " exceeded\n";
		resize(primes[++prime_index]);
	}

	if (!rebuilding) { 
		--rebuild_window;
		if (rebuild_window <= 0) return result::REBUILD;
	} 

	return result::SUCCESS;
}

template<typename K, typename V>
bool
graveyard_aos<K, V>::query(K k, V *v) 
{
	uint32_t slot;
	++queries;

	if (probe(k, &slot, QUERY)) {
		*v = value(slot);
		return true;
	}
	
	++failed_queries;
	return false;
}

template<typename K, typename V>
graveyard_aos<K, V>::result
graveyard_aos<K, V>::remove(K k)
{
	uint32_t slot;
	++removes;	

	if (probe(k, &slot, REMOVE)) {
		settomb(slot);
		++tombs;
		--records;
		--rebuild_window;
		if(rebuild_window > 0) 
			return result::SUCCESS;
		else 
			return result::REBUILD;
	}

	++failed_removes;
	return result::FAILURE;
}


template<typename K, typename V>
void
graveyard_aos<K,V>::reset_rebuild_window()
{
	rebuild_window = buckets/4.0 * (1.0 - load_factor()); // 1-a = 1/x
}

template<typename K, typename V>
void
graveyard_aos<K, V>::rebuild()
{
	int tombcount = (buckets/2) * (1.0 - load_factor()); // 1-a = 1/x
	double interval = tombcount ? (buckets / tombcount) : buckets;

	// save the part of the table that wrapped for reinsertion later
	std::vector<record_t> overflow;
	for(uint32_t p = 0; p < table_head; ++p) 
		if (full(p)) {
			overflow.push_back(table[p]);
			--records;
			settomb(p);
		}
	table_head = 0;

	boost::circular_buffer<record_t> queue(tombcount);
	for(uint32_t p = 0, q = 1, x = interval; p < buckets; p++) {
		if (--x == 0) {
			if (full(p)) queue.push_back(table[p]);
			max_rebuild_queue = std::max(max_rebuild_queue,
			                             (int)queue.size());
			settomb(p);
			x = interval;
		} else {
			if (queue.empty()) {
				if (tomb(p)) {
					while(q < buckets && !full(q)) q++;
					if (q < buckets) {
						if (hash(key(q)) > p)
							setempty(p);
						else {
							table[p] = table[q];
							settomb(q);
						}
					} else
						setempty(p);
				}
			} else {
				if (full(p)) queue.push_back(table[p]);
				table[p] = queue.front();
				queue.pop_front();
			}
		}
		if (q <= p) q = p + 1;
	}

	for (record_t r : queue) insert(r.key, r.value, true);
	for (record_t r : overflow) insert(r.key, r.value, true);
	reset_rebuild_window();	
	++rebuilds;
}

template<typename K, typename V>
void
graveyard_aos<K, V>::update_misses(uint64_t misses, enum optype op)
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
		case REBUILD_INS:
			rebuild_insert_misses += misses; break;
	}
	if (misses > longest_search) longest_search = misses;

	miss_running_avg =
	    miss_running_avg * (double)(n-1)/n + (double)misses/n;
}

template<typename K, typename V>
void
graveyard_aos<K, V>::reset_perf_counts()
{
	inserts = queries = removes = rebuild_inserts = 0;
	insert_misses = query_misses = remove_misses = 0;
	insert_shifts = 0;
	rebuild_insert_misses = 0;
	failed_inserts = failed_removes = failed_queries = duplicates = 0;
	max_rebuild_queue = 0;
	longest_search = 0;
	total_misses = 0;
	miss_running_avg = 0;
	search_count = 0;
	resizes = 0;
}

template<typename K, typename V>
void
graveyard_aos<K, V>::report_testing_stats(std::ostream &os, bool verbose)
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
template<typename K,typename V>
void
graveyard_aos<K,V>::cluster_len(std::map<int,int> *clust) const
{
	uint32_t last_empty, last_tomb; 
	last_empty = last_tomb = table_head;
	for(uint32_t p = table_head; p < buckets; ++p) {
		if (!full(p)) {
			int dist = std::min(p - last_empty, p - last_tomb);
			if (dist > 1) (*clust)[dist-1]++;
			if (empty(p)) last_empty = p;
			if (tomb(p)) last_tomb = p;
		}
	}

	// keep counting once we wrap the table
	for(uint32_t p = 0; p < table_head; ++p) {
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
template<typename K, typename V>
void
graveyard_aos<K,V>::shift_distance(std::map<int,int> *disp) const
{
	for(uint32_t p = 0; p < buckets; ++p) {
		if (full(p)) {
			uint32_t h = hash(key(p));
			int d = (p >= table_head ? p - h : buckets - h + p);
			if (d < 0)
				std::cerr << "Negative shift length at slot "
				<< p << "!\n";
			(*disp)[d]++;
		}
	 }
}

// ensure keys are monotonically increasing
template<typename K, typename V>
bool
graveyard_aos<K,V>::check_ordering()
{
	uint32_t p = table_head, q;
	bool wrapped = false, res = true;

	while (table[p].state != FULL) ++p;
	q = p;
	while(1) {
		do {
			if (++q == buckets) { 
				q = 0;
				wrapped = true;
			}
		} while (table[q].state != FULL);

		if (wrapped && q >= table_head) break;

		if (hash(key(p)) > hash(key(q))) {
			std::cerr << "Ordering violated at slot " << q << "\n";
			res = false;	
		}

		p = q;
	}

	return res;
}

template<typename K, typename V>
void
graveyard_aos<K, V>::dump()
{
	for(uint32_t i=0; i<buckets; i++) {
		if ((i!=0) && (i%10 == 0)) std::cout << "\n";
		std::cout.width(4);
		std::cout << i << ':';

		if (tomb(i)) std::cout << "\e[1;31m";
		if (i==table_head)
			std::cout << "\033[0;22m*\033[0m[";
		else
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
