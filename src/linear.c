#include <iostream>
#include <cassert>
#include "linear.h"
#include "primes.h"

using std::cerr, std::size_t;

linear::linear(size_t b) : hashtable(b)
{
	reset_rebuild_window();
	rebuilds = 0;
	tombs = 0;
	disable_rebuilds = false;
}	

bool 
linear::insert(record *t, int key, int value, bool rebuilding) 
{
	int h = hash(key);
	bool res = false;
	int miss = 1;

	if (records>=buckets) {
		cerr << "table full\n";
		failed_inserts++;
		return false;
	}

	if (rebuilding) rebuild_inserts++; else inserts++;
	for(;;) {
		if ((miss == buckets) || (t[h].state == FULL && t[h].key == key))
		{
			failed_inserts++;
			duplicates++;
			break;
		}
		else
		{
			if (t[h].state != FULL) {
				if (t[h].state == DELETED) --tombs;
				t[h] = {key, value, FULL};
				records++;
				res = true;
				break;
			}
		}
		++h;
		if (h == buckets) h=0; 
		miss++;
	}
		
	rebuilding ? ++rebuild_inserts : ++inserts;
	if (miss) update_misses(miss, rebuilding ? REBUILD : INSERT);

	if (res && load_factor() > max_load_factor) { 
		cerr << "load factor " << max_load_factor << " exceeded\n";
		resize(primes[++prime_index]);
		tombs = 0;
	}


	if (!rebuilding) { 
		--rebuild_window;
		if (rebuild_window <= 0 && !disable_rebuilds) rebuild();
	}

	return res;
}

bool 
linear::query(int key, int *value) 
{
	int h0 = hash(key);
	int miss = 0;
	bool res = false;

	queries++;
	int h = h0;
	do {
		if (table[h].state == EMPTY) break;
	
		if (table[h].state == FULL && table[h].key == key) {
			*value = table[h].value;
			res = true;
			break;
		}
		++h;
	    if (h == buckets) h=0;
		miss++;
	} while (h != h0);

	if (res == false) failed_queries++;
	if (miss) update_misses(miss, QUERY);
	return res;
}

bool
linear::remove(int key)
{
	int h0 = hash(key);
	bool res = false;
	int miss = 0;

	removes++;	
	int h = h0;
	do {
		if (table[h].state == EMPTY) break;
		
		if (table[h].state == FULL && table[h].key == key) {
			table[h].state = DELETED;
			++tombs;
			--records;
			res = true;
			break;
		}

		++h;
		if (h == buckets) h=0;
		miss++;
	} while (h != h0);

	if (res == false) failed_removes++;

	if (miss) update_misses(miss, REMOVE);
	return res;
}


void
linear::reset_rebuild_window()
{
	rebuild_window = buckets/2 * (1.0 - load_factor());
}

void
linear::rebuild()
{
}


