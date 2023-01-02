#include <iostream>
#include <cstring>
#include <vector>
#include <queue>
#include "ordered.h"
#include "primes.h"

using std::cerr, std::size_t, std::max;

ordered::ordered(size_t b) : hashtable(b)
{
	reset_rebuild_window();
	rebuilds = 0;
	table_start = 0;		// index of the first record
	disable_rebuilds = false;
}	

bool
ordered::insert(record *t, int key, int value, bool rebuilding)
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
		if (t[p].state == EMPTY)
//			t[p].state == DELETED && t[p].key >= h0)
		{
			// found an empty slot or a usable tombstone
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
		else if (hash(t[p].key) > h0 || wrapped && p == table_start)
		{
			// shift to make room for the new record
			int q = p;
			do 
				if (++q == buckets) q=0;
			while (t[q].state == FULL);
			
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
		
		if (!disable_rebuilds && !rebuilding && rebuild_window <= 0) rebuild();
	} else 
		++failed_inserts;

	// update statistics 
	rebuilding ? ++rebuild_inserts : ++inserts;
	if (miss) update_misses(miss, rebuilding ? REBUILD : INSERT);

	return result;
}

bool
ordered::query(int key, int *value)
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
			p = 0;
			wrapped = true;
		}
		miss++;
	} 

	if (res == false) failed_queries++;
	if (miss) update_misses(miss, QUERY);
	return res;
}

bool
ordered::remove(int key)
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
			++failed_removes;
			break;
		} else
		if (table[p].state == FULL && table[p].key == key) {
			--records;
			table[p].state = DELETED;
			res = true;
			break;
		}

		++p;
		if (p == buckets) {
			p = 0;
			wrapped=true;
		}
		++miss;
	} 
	
	if (res == false) failed_removes++;
	if (miss) update_misses(miss, REMOVE);
	return res;
}

void
ordered::reset_rebuild_window()
{
	// the rebuild window gets longer as load factor increases
	rebuild_window = buckets * (1.0 - load_factor()) / 2.0;
	//cerr << "Rebuild window " << rebuilds << ": " << rebuild_window << "\n";
}

/*
void
ordered::rebuild()
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

	int p = 0, q = 0;
	int blocksize = 32;
	record *recs = new record[blocksize];
	int b, x;
	do {
		b = 0;
		while (b < blocksize && q < buckets) {
			if (table[q].state == FULL) recs[b++] = table[q];
			table[q].state = EMPTY;
			++q;
		}

		// place the keys back in the table, in the order they were collected
		x = 0;
		while(x < b && p < q) {
			int h = hash(recs[x].key);
			if (p < h) p = h;
			if (table[p].state != EMPTY) ++p;
			if (p == q) break;
			table[p] = recs[x];
			++x;
			++p;
		}
	} while (q < buckets);
	delete[] recs;

	// manually reinsert the old overflow
	for (int i=0; i<overflow.size(); i++) {
		insert(table, overflow[i].key, overflow[i].value, true);
	}

	++rebuilds;
	reset_rebuild_window();	
}
*/

void
ordered::rebuild()
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

	for(int p = 0, q = 0; p < buckets; ++p, ++q) {
		if (table[p].state != FULL) {
			while (table[q].state != FULL && q < buckets) {
				table[q].state = EMPTY;
				++q;
			}
			if (q == buckets) break;

			int h = hash(table[q].key);
			if (p < h) p = h;
			if (p != q) {
				table[p] = table[q];
				table[q].state = EMPTY;
			}
		}
	}

	// reinsert the table overflow.
	for (int i = 0; i < overflow.size(); i++) {
		record x = overflow[i];
		insert(table, x.key, x.value, true);
	}

	++rebuilds;
	reset_rebuild_window();	
}

void
ordered::reset_perf_counts()
{
	rebuilds = 0;
	hashtable::reset_perf_counts();
}

void
ordered::report_testing_stats(std::ostream &os)
{
	hashtable::report_testing_stats(os);
	os << "Rebuilds: " << rebuilds << "\n";
}

// ensure keys are monotonically increasing
bool
ordered::check_ordering()
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
