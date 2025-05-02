#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cassert>

#include "graveyard.h"
#include "ordered.h"
#include "linear.h"

#define SIZE 200
#define INFINITE	/* test in an infinite loop, breaking only on error */
#define READD_BEFORE	/* do a readd test before rebuilding */

using hashtable = graveyard_soa<uint32_t,uint32_t>;
using result = hashtable::result;

bool check(hashtable &h)
{
	if (!h.check_ordering()) {
		std::cout << "Elements lost ordering" << std::endl;
		return false;
	} else if (h.failed_queries > 0) {
		std::cout << "Some data got lost" << std::endl;
		return false;
	}

	return true;
}

void
display(hashtable &h, const char *message)
{
	std::cout << "\n" << message << "\n---------\n";
	h.dump();
	std::cout << "\nrecords: " << h.num_records() << "/" << h.table_size();
	std::cout << "\n\n";
}

int
main()
{
	using std::uniform_int_distribution, std::mt19937;
	std::random_device dev;
	mt19937 rng(dev());
	const std::size_t max = std::numeric_limits<uint32_t>::max();
	uniform_int_distribution<mt19937::result_type> testset(0,max);
	int run = 0;

#ifdef INFINITE
	while (1) {
#endif
		hashtable t(SIZE);
		std::vector<uint32_t> keys(SIZE,0);

		t.disable_rebuilds = true;
		std::cout << t.table_type() << ": start run #" << ++run << "\n";

		// fill up the table completely
		t.set_max_load_factor(1.0);
		for(int i=0; i<SIZE; i++) {
			uint32_t k = testset(rng);
			result r = t.insert(k,k*2);
			switch(r) {
			case result::SUCCESS: // fall through
			case result::REBUILD: keys[i]=k; break;
			case result::DUPLICATE: --i; break;
			case result::FULLTABLE: std::cerr << "Table full\n";
			                        return 1;
			default: break;
			}
		}
		display(t, "initialize:");
		if (!check(t)) return 1;

		// remove half the items
		for(int i=0; i<SIZE/2; ++i) {
			uniform_int_distribution<> pick(0,keys.size()-1);
			int p = pick(rng);
			result r = t.remove(keys[p]);
			if(r == result::FAILURE) {
				std::cout << "failed remove [" << p << "]:"
				     << keys[p] << "!\n";
				exit(1);
				--i;
			} else {
				std::swap(keys[p],keys.back());
				keys.pop_back();
			}
		}
		display(t, "remove:");
		if (!check(t)) return 1;
#ifdef READD_BEFORE
		// add records back before the rebuild
		for(int i=0; i<SIZE/5; i++) {
			int k = testset(rng);
			if (t.insert(k,k*2) != result::FAILURE)
				keys.push_back(k);
			else
				--i;
		}
		display(t, "readd before rebuild");
		if (!check(t)) return 1;
#endif

		// perform a rebuild
		t.rebuild();
		display(t, "rebuilt");
		if (!check(t)) return 1;

		// add some more records back after the rebuild
		for(int i=0; i<SIZE/5; i++) {
			int k = testset(rng);
			result r = t.insert(k,k*2);
			switch(r) {
			case result::SUCCESS: // fall through
			case result::REBUILD: keys[i]=k; break;
			case result::DUPLICATE: --i; break;
			case result::FULLTABLE: std::cerr << "Table full\n";
			                        return 1;
			default: break;
			}
		}
		display(t, "readded after rebuild");

		t.rebuild();
		display(t, "rebuilt again");
		// check if any keys got lost
		for (size_t i=0; i<keys.size(); i++) {
			uint32_t x;
			if (!t.query(keys[i],&x)) {
				std::cout << "Lost key " << keys[i] << "\n";
			}
		}

		// random query test
		for(int i=0; i<20000; i++) {
			uniform_int_distribution<> pick(0,keys.size()-1);
			int p = pick(rng);
			uint32_t x;
			t.query(keys[p], &x);
			assert(x == keys[p] * 2);
		}

		display(t, "final");
		std::cout << "\n\n";
		if (!check(t)) return 1;

#ifdef INFINITE
	}
#endif
	return 0;
}
