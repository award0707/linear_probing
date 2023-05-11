#include <iostream>
#include <cstring>
#include <vector>
#include <queue>
#include "graveyard.h"
#include "primes.h"

using std::cerr, std::size_t, std::max;

graveyard::graveyard(size_t b) : hashtable(b)
{
	reset_rebuild_window();
	rebuilds = 0;
	table_start = 0;		// index of the first record
	disable_rebuilds = false;
}	

bool
graveyard::insert(record *t, int key, int value, bool rebuilding)
{
	int p, h0;
	int miss = 1;
	bool result = false, wrapped = false;

	if (records>=buckets) { // table full
		++failed_inserts;
		return false;
	}

	h0 = hash(key); // probe the table, starting at t[hash(key)]
	p = max(table_start,h0);
	while(1) {
		if (t[p].state != FULL)
		{
			// found an empty slot
			if (wrapped && p == table_start) ++table_start;
			result = true;
			break;
		}
		else if (t[p].state == FULL && t[p].key == key)
		{
			// duplicate key
			++duplicates;
			break;
		}
		else if (t[p].state == FULL && hash(t[p].key) > h0 ||
				 wrapped && p == table_start)
		{
			// shift to make room for the new record
			// if in a rebuild, don't use tombstones yet
			int q = p;
			while (t[q].state == FULL || rebuilding && t[q].state == DELETED)
				if (++q == buckets) q=0;
			
			if (q < p) {	// if the shift wrapped around
				std::memmove(&t[1], &t[0], sizeof(record)*q);
				t[0] = t[buckets-1];
				std::memmove(&t[p+1], &t[p], sizeof(record)*(buckets-1-p));
			} else 
				std::memmove(&t[p+1], &t[p], sizeof(record)*(q-p));

			if ((q < p || wrapped) && q >= table_start) ++table_start;
			result = true;
			break;
		}

		++p;
		if (p == buckets) {
			p=0;
			wrapped=true;
		}
		++miss;
	}	

	// insert the new key at position p
	if (result == true) {
		t[p] = {key,value,FULL};
		++records;

		if (!rebuilding) --rebuild_window;

		if (load_factor() > max_load_factor) {
			cerr << "load factor " << max_load_factor << " exceeded\n";
			resize(primes[++prime_index]);
		}

		if (!rebuilding && rebuild_window <= 0 && !disable_rebuilds) rebuild();
	} else 
		++failed_inserts;

	// update statistics 
	rebuilding ? ++rebuild_inserts : ++inserts;
	if (miss) update_misses(miss, rebuilding ? REBUILD : INSERT);

	return result;
}

bool
graveyard::query(int key, int *value)
{
	int p, h0;
	bool res = false;
	bool wrapped = false;
	int miss = 1;

	queries++;
	h0 = hash(key);
	p = max(table_start,h0);
	while(1) {
		if (table[p].state == EMPTY ||
			table[p].state == FULL && hash(table[p].key) > h0 ||
			wrapped && p == table_start) 
			break;

		if (table[p].state == FULL && table[p].key == key) {
			*value = table[p].value;
			res = true;
			break;
		}
		++p;
		if (p == buckets) {
			p=0;
			wrapped=true;
		}
		miss++;
	} 

	if (res == false) failed_queries++;
	if (miss) update_misses(miss, QUERY);
	return res;
}

bool
graveyard::remove(int key)
{
	int p, h0;
	bool res = false, wrapped = false;
	int miss = 1;

	removes++;	
	h0 = hash(key);
	p = max(table_start,h0);
	while(1) {
		if (table[p].state == EMPTY ||
			table[p].state == FULL && hash(table[p].key) > h0 ||
			wrapped && p == table_start) {
			failed_removes++;
			break;
		}

		if (table[p].state == FULL && table[p].key == key) {
			records--;
			table[p].state = DELETED;
			rebuild_window--;			// deletions contribute to rebuilds
			res = true;
			break;
		}

		++p;
		if (p == buckets) {
			p=0;
			wrapped=true;
		}
		++miss;
	} 

	if (res == false) failed_removes++;
	if (miss) update_misses(miss, REMOVE);
	if (rebuild_window <= 0 && !disable_rebuilds) rebuild();
	return res;
}

void
graveyard::reset_rebuild_window()
{
	// the rebuild window gets smaller as load factor increases
	// R = n/(4x) = n/4 * (1-lf)
	// for graveyard, it's half the size of the window used for ordered probing
	rebuild_window = buckets/4 * (1.0 - load_factor());
//	cerr << "Rebuild window " << rebuilds << ": " << rebuild_window << ", ";
}



void
graveyard::rebuild()
{
	std::vector<record> overflow;

	// temporarily save the table overflow 
	for(int p = 0; p < table_start; ++p) {
		if (table[p].state == FULL) {
			overflow.push_back(table[p]);
			--records;
		}
		table[p].state = EMPTY;
	}
	table_start = 0;

	int tombcount = buckets/2*(1.0-load_factor());
	int interval = 2.0 / (1.0 - load_factor());
	record *aux = new record[tombcount];
	int top = -1;
	
	int p, q;
	for(p = 0, q = 1; p < buckets; p++) {
		if (p % interval == 0) {
			if (table[p].state == FULL) aux[++top] = table[p];
			table[p].state = DELETED;
			p++;
		} else {
			if (table[p].state != FULL) {
			}
			if (top != -1) {
				;
			} else {
				while(q < buckets && table[q].state != FULL) q++;
				if (q == buckets) break;
				if (hash(table[q].key) <= p) {
					table[p] = table[q];
					table[q].state = EMPTY;
				} else {
					table[p].state = EMPTY;
				}
			}
		}
		if (p > q) cerr << "LELZ\n";
	}

	// clear to the end of table, then manually reinsert any remaining elements
	while (p < buckets) {
		if (p % interval == 0)
			table[p].state = DELETED;
		else
			table[p].state = EMPTY;
		++p;
	}

	while (top > -1) {
		insert(table, aux[top].key, aux[top].value, true);
		--records;
		--top;
	}

	for (int i = 0; i < overflow.size(); i++) {
		record r = overflow[i];
		insert(table, r.key, r.value, true);
	}

	++rebuilds;
	reset_rebuild_window();	
	delete aux;
}

void
graveyard::reset_perf_counts()
{
	rebuilds = 0;
	hashtable::reset_perf_counts();
}

void
graveyard::report_testing_stats(std::ostream &os)
{
	hashtable::report_testing_stats(os);
	os << "Rebuilds: " << rebuilds << "\n";
}

// ensure keys are monotonically increasing
bool
graveyard::check_ordering()
{
	int p = table_start, q;
	bool wrapped = false;

	while (table[p].state != FULL) ++p;
	q = p;
	while(1) {
		while (table[++q].state != FULL)
			if (q == buckets) { 
				q = 0;
				wrapped = true;
			}

		if (wrapped && q >= table_start) break;

		if (hash(table[p].key) > hash(table[q].key)) {
			std::cerr << "Ordering violated at slot " << q << "\n";
			return false;
		}

		p = q;
	}

	return true;
}
