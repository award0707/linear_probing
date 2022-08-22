#include <iostream>
#include "linear.h"
#include "primes.h"

using std::cerr, std::size_t;

bool 
linear::insert(record *t, int key, int value, bool rebuilding) 
{
	int h = hash(key);
	bool res = false;
	int miss = 0;

	if (records>=buckets) {
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
			if (t[h].state == EMPTY || t[h].state == DELETED) {
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
		
	if (miss) update_misses(miss, rebuilding ? REBUILD : INSERT);

	if (res && load_factor() > max_load_factor) { 
		cerr << "load factor " << max_load_factor << " exceeded\n";
		resize(primes[++prime_index]);
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
			records--;
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


