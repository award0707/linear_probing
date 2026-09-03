#include <iostream>
#include <cassert>
#include <cstring>
#include "graveyard.h"
#include "primes.h"
#include <boost/circular_buffer.hpp>

using std::cerr, std::size_t;

graveyard::
graveyard(uint32_t n, uint32_t b)
{
	prime_index = 0;
	while(n > primes[prime_index]) 
		prime_index++;
	
	table = new record[n];
	if (!table) cerr << "Couldn't allocate table\n";
	states = new enum slot_state[n];
	if (!states) cerr << "Couldn't allocate states\n";
	
	for(uint32_t i=0; i<n; i++) states[i] = EMPTY;

	slots = n;
	blocksize = b;
	blockcount = n / b + (n % b != 0);
	fullcount = 0;
	tombcount = 0;
	table_head = 0;
	keycount = 0;
	disable_rebuilds = false;
	max_load_factor = 0.5;

	miss_running_avg = 0;
	search_count = 0;
	total_misses = 0;
	rebuilds = 0;

	reset_perf_counts();
	reset_rebuild_window();
}	

graveyard::
~graveyard()
{
	delete[] table;
	delete[] states;
}

uint32_t graveyard::
hash(uint32_t k) const
{
	return (uint32_t)(((uint64_t)k * (uint64_t)slots) >> 32);
}

void graveyard::
resize(uint32_t n)
{
	uint32_t oldslots = slots;
	record *oldtable = table;
	slot_state *oldstates = states;

	cerr << "resize(): rehashing into " << b << " slots\n";
	
	table = new record[n];
	if (!table) cerr << "resize: couldn't allocate table\n"; 
	states = new enum slot_state[n];
	if (!states) cerr << "resize: couldn't allocate states\n";
	for(uint32_t i=0; i<n; ++i) 
		states[i] = EMPTY;
	keycount = 0;
	tombcount = 0;
	slots = n;

	for(uint32_t i=0; i<oldslots; ++i) 
		if (oldstates[i] == FULL)
			insert(oldtable[i].key, oldtable[i].value);

	delete[] oldtable;
	delete[] oldstates;
	resizes++;	
}

bool graveyard::
probe(uint32_t k, uint32_t *slot)
{
	const uint32_t h = hash(k);
	uint64_t miss = 0;
	bool res = false;
	uint32_t s = std::max(h, table_head);

	res = false;
	while(1) {
		if (full(s) && key(s) == k) {
			res = true;
			break;
		}
		if (empty(s) || (full(s) && hash(key(s)) > h)) break;
		if (++s == slots) s = 0;
		++miss;
		if (s == table_head) break;
	}

	if (miss) update_misses(miss, operation);
	*slot = s;
	return res;
}

bool graveyard::
insert_probe(uint32_t k, uint32_t *slot, bool* wrapped)
{
	const uint32_t h = hash(k);
	uint64_t miss = 0;
	bool res = false;
	uint32_t s = std::max(h, table_head);

	res = true;
	while(1) {
		if (full(s) && key(s) == k) {
			res = false; // duplicate key not allowed
			break;
		}
		if (empty(s) || (full(s) && hash(key(s)) > h)) break;
		if (++s == slots) {
			s = 0;
			*wrapped = true;
		}
		++miss;
		if (s == table_head) break; 
	}
	/* // rescan this hash for dupes if ordering is not strict */
	/* t = s; */
	/* while(1) { */
	/* 	if (t == table_head) break; */
	/* 	if (full(t) && key(t) == k) { */
	/* 		res = false; */
	/* 		break; */
	/* 	} */
	/* 	if (++t == slots) t = 0; */
	/* 	if (empty(t) || hash(key(t)) != h) */
	/* 		break; */
	/* } */
	if (miss) update_misses(miss, operation);
	*slot = s;
	return res;
}


inline void graveyard::
slotmove(uint32_t destidx, uint32_t srcidx, size_t count)
{
	std::memmove(&table[destidx], &table[srcidx], sizeof(record) * count);
	std::memmove(&states[destidx], &states[srcidx],
	             sizeof(enum slot_state) * count);
}

// find the end of the cluster, then slide records 1 to the right as a block
uint32_t graveyard::
shift(uint32_t start)
{
	using std::memmove;
	const uint32_t last = slots-1;
	uint32_t end = start;
	
	do
		if (++end > last) end = 0;
	while (full(end));

	if (tomb(end)) --tombcount; // made use of a tombstone

	if (end < start) {
		slotmove(1, 0, end);
		slotmove(0, last, 1);
		slotmove(start+1, start, last-start);
	} else
		slotmove(start+1, start, end-start);

	return end;
}


int graveyard::
rb_seek(uint32_t x, uint32_t &end)
{
	const uint32_t last = slots-1;
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

uint32_t graveyard::
rb_shift(uint32_t start)
{
	record lastscratch, scratch;
	enum slot_state lastscratch_state, scratch_state;
	bool valid = false;
	uint32_t end;

	while(1) {
		int res = rb_seek(start, end);

		scratch = table[end];
		scratch_state = states[end];

		slotmove(start+1, start, end-start);
		if (valid) {
			table[start] = lastscratch;
			states[start] = lastscratch_state;
		}
		lastscratch = scratch;
		lastscratch_state = scratch_state;
		valid = true;

		if (res == 0) break;
		start = end + 2;
		if (res == 2 || (res == 1 && start > slots-1)) {
			end = rb_shift(0);
			table[0] = lastscratch;
			states[0] = lastscratch_state;
			break;
		}
	}

	return end;
}

graveyard::result graveyard::
insert(uint32_t k, uint32_t v)
{
	uint32_t slot;
	bool wrapped=false;

	if (keycount>=slots) {
		++failed_inserts;
		return result::FULLTABLE;
	}

	if (!insert_probe(k, &slot, &wrapped)) {
		++failed_inserts;
		++duplicates;
		return result::DUPLICATE;
	}

	if (!empty(slot)) {
		uint32_t end = shift(slot);
		if (((end < slot) || wrapped) && end >= table_head)
			++table_head;
		if (end >= slot)
			insert_shifts += end - slot;
		else
			insert_shifts += (slots - slot + end);
	} else if (wrapped && slot == table_head)
		table_head++;
	
	setkey(slot, k);
	setvalue(slot, v);
	setfull(slot);
	
	++keycount;
	++inserts;
	
	--rebuild_window;
	if (rebuild_window <= 0)
		return result::REBUILD;
	else
		return result::SUCCESS;
}

graveyard::result graveyard::
rb_insert(uint32_t k, uint32_t v)
{
	uint32_t slot;
	bool wrapped=false;

	if (keycount>=slots) {
		++failed_inserts;
		return result::FULLTABLE;
	}

	if (!insert_probe(k, &slot, &wrapped)) {
		++failed_inserts;
		++duplicates;
		return result::DUPLICATE;
	}

	if (!empty(slot)) {
		uint32_t end = rb_shift(slot);
		if (((end < slot) || wrapped) && end >= table_head)
			++table_head;
	} else if (wrapped && slot == table_head)
		table_head++;
	
	if (tomb(table_head)) ++table_head;
	setkey(slot, k);
	setvalue(slot, v);
	setfull(slot);
	
	++keycount;
	++rebuild_inserts;

	return result::SUCCESS;
}


bool graveyard::
query(uint32_t k, uint32_t *v) 
{
	uint32_t slot;
	++queries;

	if (probe(k, &slot)) {
		*v = value(slot);
		return true;
	}
	
	++failed_queries;
	return false;
}

graveyard::result graveyard::
remove(uint32_t k)
{
	uint32_t slot;
	++removes;	

	if (probe(k, &slot)) {
		settomb(slot);
		++tombcount;
		--keycount;
		--rebuild_window;
		if(rebuild_window > 0) 
			return result::SUCCESS;
		else 
			return result::REBUILD;
	}

	++failed_removes;
	return result::FAILURE;
}


void graveyard::
reset_rebuild_window()
{
	rebuild_window = slots/4.0 * (1.0 - load_factor()); // 1-a = 1/x

}

void graveyard::
rebuild()
{
	int tombcount = (slots/2) * (1.0 - load_factor()); // 1-a = 1/x
	double interval = tombcount ? (slots / tombcount) : slots;

	// save the part of the table that wrapped for reinsertion later
	std::vector<struct record> overflow;
	for(uint32_t p = 0; p < table_head; ++p) 
		if (full(p)) {
			overflow.push_back(table[p]);
			--keycount;
			settomb(p);
		}
	table_head = 0;
	tombcount = 0;

	boost::circular_buffer<struct record> queue(tombcount);
	for(uint32_t p = 0, q = 1, x = interval; p < slots; ++p) {
		if (!--x) {
			x = interval;
			if (full(p)) queue.push_back(table[p]);
			settomb(p);
			++tombcount;
			max_rebuild_queue = std::max(max_rebuild_queue,
			                             (int)queue.size());
		} else if (queue.empty() && tomb(p)) {
			if (q <= p) q = p + 1;
			while(q < slots && !full(q)) ++q;
			if (q < slots && hash(key(q)) <= p) {
				table[p] = table[q];
				setfull(p);
				settomb(q);
				++tombcount; 
			} else
				setempty(p);
		} else if (!queue.empty()) {
			if (full(p)) queue.push_back(table[p]);
			table[p] = queue.front();
			setfull(p);
			queue.pop_front();
		}
	}

	keycount -= queue.size(); // avoid double count on reinsert
	for (record r : queue) rb_insert(r.key, r.value);
	for (record r : overflow) rb_insert(r.key, r.value);
	reset_rebuild_window();	
	++rebuilds;
}

void graveyard::
update_misses(uint64_t misses, enum optype op)
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

void graveyard::
reset_perf_counts()
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

void graveyard::
report_testing_stats(std::ostream &os, bool verbose)
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
void graveyard::
cluster_len(std::map<int,int> *clust) const
{
	uint32_t last_empty, last_tomb; 
	last_empty = last_tomb = table_head;
	for(uint32_t p = table_head; p < slots; ++p) {
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
				(slots - last_empty + p) : (p - last_empty);
			int y = last_tomb >= table_head ?
				(slots - last_tomb + p) : (p - last_tomb);
			int dist = std::min(x, y);
			if (dist > 1) (*clust)[dist-1]++;
			if (empty(p)) last_empty = p;
			if (tomb(p)) last_tomb = p;
		}
	}
}

// fill in a histogram of search distances
// i.e. the distance from a key's slot and the hash of that key
void graveyard::
search_distance(std::map<int,int> *disp) const
{
	for(uint32_t p = 0; p < slots; ++p) {
		if (full(p)) {
			uint32_t h = hash(key(p));
			int d = (p >= table_head ? p - h : slots - h + p);
			if (d < 0)
				std::cerr << "Negative shift length at slot "
				<< p << "!\n";
			(*disp)[d]++;
		}
	 }

	int ntile = keycount/5;
	int total=0, which=0, last=0;
	for (auto iter = disp->begin(); iter != disp->end(); ++iter) { 
		std::cerr << iter->first << ":" << iter->second << " ";
		total += iter->second;
		if (total > ntile || std::next(iter) == disp->end()) {
			std::cerr << which+1 << "th quintile " << 
				last << " - " << iter->first << "\n";
			total -= ntile;
			last = iter->first;
			which++;
		}
	}
}

// ensure keys are monotonically increasing
bool graveyard::
check_ordering()
{
	uint32_t p = table_head, q;
	bool wrapped = false, res = true;

	while (states[p] != FULL) ++p;
	q = p;
	while(1) {
		do {
			if (++q == slots) { 
				q = 0;
				wrapped = true;
			}
		} while (states[q] != FULL);

		if (wrapped && q >= table_head) break;

		if (hash(key(p)) > hash(key(q))) {
			std::cerr << "Ordering violated at slot " << q << "\n";
			res = false;	
		}

		p = q;
	}

	return res;
}

void graveyard::
debug_key_search(uint32_t k)
{
	uint32_t x, b; 
	bool found = false;

	for(uint32_t i=0; i<slots; i++)
		if (key(i) == k) {
			x = i;
			uint32_t j = i;
			while (!empty(j)) j--;
			b = j;
			found = true;
			break;
		}
	
	if (found) {
		std::cerr << "found it in slot " << x << "!\n";
		if (states[x] == TOMB) {
			std::cerr << "it is marked as a tombstone\n";
		} else if (states[x] == EMPTY) {
			std::cerr << "it is marked empty\n";
		}
		std::cerr << "empty before it: " << b << ".\n";
		std::cerr << "actual hash " << hash(k) << "\n";
		std::cerr << "table head: " << table_head << "\n";
	} else
		std::cerr << "it's not actually in the table\n";

	if (!check_ordering())
		std::cerr << "Ordering was violated\n";
}

void graveyard::
dump()
{
	for(uint32_t i=0; i<slots; i++) {
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
