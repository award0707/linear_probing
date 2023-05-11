#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include "ordered.h"
#include "linear.h"
#include "graveyard.h"

#define SIZE 100
#define TABLETYPE linear
#define INFINITE		/* test in an infinite loop, breaking only on error */
//#define READD_BEFORE	/* do a readd test before rebuilding */

using namespace std;

void display(TABLETYPE &h, const char *message);

int
main()
{
	random_device dev;
	mt19937 rng(dev());
	uniform_int_distribution<mt19937::result_type> testset(0,9999);
	uniform_int_distribution<mt19937::result_type> secondset(10000-SIZE-5,9999);

#ifdef INFINITE
	while (1) {
#endif
		TABLETYPE t(SIZE);
		vector<int> keys(SIZE-5,0);

		t.disable_rebuilds = true;
		cout << "Start\n";

		// fill up the table completely
		t.set_max_load_factor(1.0);
		for(int i=0; i<SIZE-5; i++) {
			int k = testset(rng);
			if (t.insert(k,i)) 
				keys[i] = k;
			else 
				--i;
		}
		display(t, "initialized");
		

		// remove half the items
		for(int i=0; i<SIZE/2; ++i) {
			uniform_int_distribution<> pick(0,keys.size()-1);
			int p = pick(rng);
			if(!t.remove(keys[p])) {
				cout << "failed remove [" << p << "]:" << keys[p] << "!\n";
				--i;
			} else {
				swap(keys[p],keys.back());
				keys.pop_back();
			}
		}
		display(t, "removed");

#ifdef READD_BEFORE
		// add records back before the rebuild
		for(int i=0; i<SIZE/5; i++) {
			int k = secondset(rng);
			if (t.insert(k,i)) 
				keys.push_back(k);
			else 
				--i;
		}
		display(t, "readded before rebuild");
#endif

		// perform a rebuild
		t.rebuild();
		display(t, "rebuilt");

		// add some more records back after the rebuild
		for(int i=0; i<SIZE/5; i++) {
			int k = secondset(rng);
			if (t.insert(k,i)) 
				keys.push_back(k);
			else 
				--i;
		}
		display(t, "readded after rebuild");

		t.rebuild();
		display(t, "rebuilt again");

		for (int i=0; i<keys.size(); i++) {
			int x;
			if (!t.query(keys[i],&x)) {
				cout << "Lost key " << keys[i] << "\n";
			}
		}
		
		for(int i=0; i<20000; i++) {
			uniform_int_distribution<> pick(0,keys.size()-1);
			int p = pick(rng);
			int x;
			t.query(keys[p], &x); 
		}
		
		display(t, "final");
		cout << "\n\n";

		if (!t.check_ordering()) {
			cout << "Elements lost ordering" << endl;
			return 1;
		} else if (t.failed_queries > 0) {
			cout << "Some data got lost" << endl;
			return 1;
		}
#ifdef INFINITE
	}
#endif
	return 0;
}

void
display(TABLETYPE &h, const char *message)
{
	cout << "\n" << message << "\n---------\n";
	dump(h);
	cout << "\nrecords: " << h.num_records() << "/" << h.table_size() << "\n";
	cout << "\n";
}

#if TABLETYPE == linear
void
dump(TABLETYPE &h)
{
	for(int i=0; i<h.buckets; i++) {
		if ((i!=0) && (i%10 == 0)) std::cout << "\n";
		std::cout.width(4);
		std::cout << i;
		std::cout << ": [";
		if(h.table[i].state == hashtable::FULL) {
			std::cout.width(4);
			std::cout << h.hash(h.table[i].key) << "]";
			std::cout.width(4);
			std::cout << h.table[i].key;
		} else if (h.table[i].state == hashtable::EMPTY) {
			std::cout << "    ]    ";
		} else {
			std::cout.width(4);
			std::cout << "____]";
			std::cout << "____";
		}
	}
}
#elif
void
dump(TABLETYPE &h)
{
	for(int i=0; i<h.buckets; i++) {
		if ((i!=0) && (i%10 == 0)) std::cout << "\n";
		std::cout.width(4);
		std::cout << i;
	    if (i==h.table_start) std::cout << ":S["; else std::cout << ": [";
		if(h.table[i].state == hashtable::FULL) {
			std::cout.width(4);
			std::cout << h.hash(h.table[i].key) << "]";
			std::cout.width(4);
			std::cout << h.table[i].key;
		} else if (h.table[i].state == hashtable::EMPTY) {
			std::cout << "    ]    ";
		} else {
			std::cout.width(4);
			std::cout << "____]";
			std::cout << "____";
		}
	}
}
#endif
