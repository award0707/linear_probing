#include <iostream>
#include "quadratic.h"
#include "primes.h"

using std::cerr, std::size_t;

bool
quadratic::insert(record *t, int key, int value, bool rebuilding)
{
	int hv = hash(key);
	bool res = false;
	int miss = 0;

	if (records>=buckets) { 
		failed_inserts++;
		return false;
	}

	rebuilding ? rebuild_inserts++ : inserts++;
	for(int i=0;;i++) {
		int h = (hv+i*i) % buckets;
		if (miss >= buckets/2)
		{
			failed_inserts++;
			break;
		}
		else if (t[h].state == FULL && t[h].key == key)
		{ 
			failed_inserts++;
			++duplicates;
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
quadratic::query(int key, int *value)
{
	int hv = hash(key);
	bool res = false;
	int miss = 0;

	queries++;
	for(int i=0;i<buckets;i++) {
		int h = (hv+i*i) % buckets;	
		if (table[h].state == EMPTY) break;
		if (table[h].state == FULL && table[h].key == key) {
			*value = table[h].value;
			res = true;
			break;
		}
		miss++;
	}

	if (res == false) failed_queries++;
	if (miss) update_misses(miss, QUERY);
	return false;
}

bool
quadratic::remove(int key)
{
	int hv = hash(key);
	bool res = false;
	int miss = 0;

	removes++;	
	for(int i=0;i<buckets;i++) {
		int h = (hv+i*i) % buckets;
		if (table[h].state == EMPTY) break;
		if (table[h].state == FULL && table[h].key == key) {
			table[h].state = DELETED;
			records--;
			res = true;
			break;
		}
		miss++;
	}
	
	if (res == false) failed_removes++;
	if (miss) update_misses(miss, REMOVE);
	return res;
}


