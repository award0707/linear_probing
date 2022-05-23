#define C_END 5000

#include "olinear.h"
#include <iostream>
#include <cstring>

kvstore::kvstore(int b) {
	prime_index = 0;
	while(b > primes[prime_index]) {
		prime_index++;
	}
	
	table = new hashnode[b];
	for(int i=0; i<b; i++) {
		table[i].status = EMPTY;
	}

	buckets = b;
	p_max_load_factor = 0.9;
	avg_find_miss = 0;
	search_count = 0;
	coll = 0;
	elements = 0;
	rehashes = 0;
	rebuild_window = (buckets * (1.0 - load_factor())) / 2.0;

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

	int b = buckets;
	int h = h0;
	
	if (elements>=buckets || h > (b-C_END)) {
		failed_insert++;
		return false;
	}

	rehashing ? rehashing_insert_c++ : insert_c++;
	for(;;) {
		if (t[h].status == EMPTY || t[h].status == DELETED) {
			t[h].data.key = key;
			t[h].data.value = value;
			t[h].status = FULL;
			elements++;
			res = true;
			if (!rehashing) rebuild_window--;
			break;
		} else if (t[h].data.key == key) { 
			failed_insert++;
			break;
		} else if (hash(t[h].data.key) > h0) {
			hashnode* start = &t[h];
			hashnode* end = start+1;
			while (end->status != EMPTY && end->status != DELETED) end++;
			std::memmove(start+1, start, end-start);	
			
			t[h].data.key = key;
			t[h].data.value = value;
			t[h].status = FULL;
			elements++;
			res = true;
			if (!rehashing) rebuild_window--;
			break;
		}
		
		h = (h+1) % buckets;
		miss++;
	}
		
	if (miss) update_misses(miss, rehashing ? REHASH : INSERT);

	if (res && load_factor() > p_max_load_factor) rehash();

	if (!rehashing && rebuild_window <= 0) rebuild(); 
	return res;
}

void kvstore::rebuild() {
	hashnode *newtable = new hashnode[buckets];
	for(int i=0; i<buckets; i++) {
		newtable[i].status = EMPTY;
	}
	elements = 0;

	for(int i=0; i<buckets; i++) {
		if (table[i].status == FULL)
			insert(newtable, table[i].data.key, table[i].data.value, true);
	}

	delete table;
	table = newtable;

	rebuilds++;	
	rebuild_window = (buckets * (1.0 - load_factor())) / 2.0;
}

bool kvstore::insert(int key, int value) {
	return insert(table,key,value);
}

int* kvstore::find(int key) {
	int h0 = hash(key);
	int* ret = nullptr;
	int miss = 0;

	find_c++;
	int h = h0;
	for(;;) {
		if (table[h].status == EMPTY || hash(table[h].data.key) > h0) {
			failed_find++;
			ret = nullptr;
			break;
		}
		if (table[h].status == FULL && table[h].data.key == key) {
			ret = &table[h].data.value;
			break;
		}
		h = (h+1) % buckets;
		miss++;
		if (h >= (buckets-C_END)) { 
			failed_find++;
			ret = nullptr;
			break;
		}	
	}

	if (miss) update_misses(miss, FIND);
	return ret;
}

bool kvstore::remove(int key) {
	int h0 = hash(key);
	bool res = false;
	int miss = 0;

	remove_c++;	
	int h = h0;
	for(;;) {
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
		h = (h+1) % buckets;
		miss++;
		if (h >= (buckets - C_END)) {
			failed_remove++;
			break;
		}
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
