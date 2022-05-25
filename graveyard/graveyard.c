#include "graveyard.h"
#include <iostream>
#include <cstring>

#define C_END 10000

kvstore::kvstore(int b) {
	prime_index = 0;
	while(b > primes[prime_index]) {
		prime_index++;
	}
	
	table = new hashnode[b+C_END];
	for(int i=0; i<b+C_END; i++) {
		table[i].status = EMPTY;
	}

	buckets = b;
	p_max_load_factor = 0.9;
	avg_find_miss = 0;
	search_count = 0;
	coll = 0;
	elements = 0;
	rehashes = 0;
	rebuilds = 0;
	rebuild_window = buckets * (1.0-load_factor())/4.0;

	insert_coll_c = find_coll_c = remove_coll_c = 0;
	rehashing_insert_coll_c = 0;
	insert_c = find_c = remove_c = 0;
	rehashing_insert_c = 0;
	failed_insert = failed_remove = failed_find = 0;
}

kvstore::~kvstore() {
	delete table;	
}

bool kvstore::insert(hashnode *t, int key, int value, bool rehashing = false) {
	int h0 = hash(key);
	bool res = false;
	int miss = 0;

	if (elements>=buckets) {
		failed_insert++;
		return false;
	}

	// make sure we didn't push anything off the end of the table
	if(t[buckets+C_END-1].status != EMPTY) {
		std::cerr << "increase C_END\n";
	}

	rehashing ? rehashing_insert_c++ : insert_c++;
	for(int h=h0; h<buckets; h++) {
		if (t[h].status == EMPTY || (t[h].status == DELETED && hash(t[h].data.key) >= h0)) {
			// found empty slot or deleted slot with an equal/higher hash
			t[h].data.key = key;
			t[h].data.value = value;
			t[h].status = FULL;
			elements++;
			res = true;
			if (!rehashing) rebuild_window--;
			break;
		} else if (t[h].status == FULL && t[h].data.key == key) { 
			// duplicate key found, don't insert
			failed_insert++;
			break;
		} else if (hash(t[h].data.key) > h0) {
			// slide keys down to make room and insert this key here with
			// others of the same hash
			hashnode *start = &t[h];
			hashnode *end = start+1;
			while (end->status != EMPTY && end->status != DELETED) end++;
			std::memmove(start+1, start, sizeof(hashnode)*(end-start));

			t[h].data.key = key;
			t[h].data.value = value;
			t[h].status = FULL;
			elements++;
			res = true;
			if (!rehashing) rebuild_window--;
			break;
		}
		
		miss++;
	}
		
	if (miss) update_misses(miss, rehashing ? REHASH : INSERT);

	if (res && load_factor() > p_max_load_factor) rehash();

	if (rebuild_window <= 0) rebuild(); 
	return res;
}

void kvstore::insert_tombstone(hashnode *t, int key) {
	int h = hash(key);

	if (t[h].status != EMPTY) {
		hashnode *start = &t[h];
		hashnode *end = start+1;
		while (end->status == FULL) end++;
		std::memmove(start+1, start, sizeof(hashnode)*(end-start));
	}
	
	t[h].data.key = key;
	t[h].data.value = 0;
	t[h].status = DELETED;
}

void kvstore::rebuild() {
	rebuilds++;	
	rebuild_window = buckets * (1.0-load_factor())/4.0;

	hashnode *newtable = new hashnode[buckets+C_END];
	for(int i=0; i<buckets+C_END; i++) {
		newtable[i].status = EMPTY;
	}

	elements = 0;
	for(int i=0; i<buckets+C_END; i++) {
		if (table[i].status == FULL)
			insert(newtable, table[i].data.key, table[i].data.value, true);
	}

	int interval = buckets * (1.0-load_factor())/2.0;
	for(int i=interval; i<buckets; i+=interval) {
		insert_tombstone(newtable, i);
	}

	delete table;
	table = newtable;

}

bool kvstore::insert(int key, int value) {
	return insert(table,key,value);
}

int* kvstore::find(int key) {
	int h0 = hash(key);
	int* ret = nullptr;
	int miss = 0;

	find_c++;
	for(int h = h0; h < buckets+C_END; h++) {
		if (table[h].status == EMPTY || hash(table[h].data.key) > h0) {
			failed_find++;
			ret = nullptr;
			break;
		}
		if (table[h].status == FULL && table[h].data.key == key) {
			ret = &table[h].data.value;
			break;
		}
		miss++;
	}

	if (miss) update_misses(miss, FIND);
	return ret;
}

bool kvstore::remove(int key) {
	int h0 = hash(key);
	bool res = false;
	int miss = 0;

	remove_c++;	
	for(int h = h0; h < buckets+C_END; h++) {
		if (table[h].status == EMPTY || hash(table[h].data.key) > h0) {
			failed_remove++;
			break;
		}
		if (table[h].status == FULL && table[h].data.key == key) {
			table[h].status = DELETED;
			elements--;
			rebuild_window--;
			res = true;
			break;
		}
		miss++;
	}

	if (miss) update_misses(miss, REMOVE);
	if (rebuild_window <= 0) rebuild();
	return res;
}

bool kvstore::get(int key, int* value) {
	int *r = find(key);
	if (!r)
		return false;
	else {
		*value = *r;
		return true; 
	}
}

bool kvstore::set(int key, int value) {
	int *r = find(key);
	if (!r) 
		return insert(key, value);
	else { 
		*r = value; 
		return true; 
	}
}

uint64_t kvstore::hash(int k) {
	int r = k % buckets;
    	return (r<0)?r+buckets:r;	
}

void kvstore::rehash() {
	std::size_t b;
	std::size_t oldbuckets = buckets;

	b = primes[++prime_index];
	std::cerr << "load factor " << p_max_load_factor << " exceeded, rehashing into " << b << " buckets\n";
	hashnode *newtable = new hashnode[b];
	for(int i=0; i<b; i++) {
		newtable[i].status = EMPTY;
	}
	elements = 0;
	buckets = b;

	for(int i=0; i<oldbuckets; i++) {
		if (table[i].status == FULL)
			insert(newtable, table[i].data.key, table[i].data.value, true);
	}

	delete table;
	table = newtable;
	rehashes++;	
}

void kvstore::set_max_load_factor(double f) {
	p_max_load_factor = f;
}

void kvstore::update_misses(int misses, enum optype op) {
	search_count++;
	coll++;
	switch(op) {
		case INSERT:
			insert_coll_c++; break;
		case FIND:
			find_coll_c++; break;
		case REMOVE:
			remove_coll_c++; break;
		case REHASH:
			rehashing_insert_coll_c++; break;
	}
	if (misses > longest_search) longest_search = misses;
	avg_find_miss = (avg_find_miss * (search_count - 1.0) + misses) / (double)search_count;
}

void kvstore::reset_counts() {
	insert_c = find_c = remove_c = rehashing_insert_c = 0;
	insert_coll_c = find_coll_c = remove_coll_c = rehashing_insert_coll_c = 0;
	failed_insert = failed_remove = failed_find = 0;
	longest_search = 0;
	avg_find_miss = 0;
	search_count = 0;
	rebuilds = 0;
}

double kvstore::load_factor() {
	return (double)elements/buckets;
}

double kvstore::avg_misses() {
	return avg_find_miss;
}

int kvstore::collision_c() {
	return insert_coll_c + find_coll_c + remove_coll_c;
}

int kvstore::bucket_c() {
	return buckets;
}

int kvstore::element_c() {
	return elements;
}
