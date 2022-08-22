#include <iostream>
#include <chrono>
#include <unistd.h>
#include <vector>
#include <random>

#include "primes.h"
#include "bench.h"
#include "linear.h"
#include "quadratic.h"
#include "ordered.h"
#include "graveyard.h"

using namespace std;

int
main(int argc, char **argv)
{
	params_t p;
	hashtable *ht; 	

	if(!setup_test_params(argc, argv, &p)) exit(1);

	switch(p.type) {
		case params_t::linear:    ht = new linear(p.table_size); break;
		case params_t::quadratic: ht = new quadratic(p.table_size); break;
		case params_t::ordered:   ht = new ordered(p.table_size); break;
		case params_t::graveyard: ht = new graveyard(p.table_size); break;
	}

	ht->set_max_load_factor(1.0);	// disable automatic resizing

	if (p.verbose) {
		locale comma_locale(locale(), new comma_numpunct());
		cout.imbue(comma_locale);
		cout << "Using table type " << ht->table_type()
			<< ", i=" << p.insert << ", f=" << p.query << ", r=" << p.remove
			<< ", fails = " << p.fails
			<< ", table size=" << ht->table_size()
			<< "(" << ht->table_size_bytes()/1000 << "KB)"
			<< ", lf=" << p.loadfactor << "\n\n";
	}

	//
	// initialization phase
	// generate random numbers and insert into the table.
	// maintain a list of all keys inserted until target load factor reached
	//
	vector<int> keys;
	stats_t loadstats = { ht, {}, 0 };
	int opscount = OPS_INTERVAL;
	random_device dev;
	mt19937 rng(dev());
	uniform_int_distribution<mt19937::result_type> data(0,1'000'000'000L);
	uniform_int_distribution<mt19937::result_type> operation(0,99); 

	cout << "Begin loading phase\n";
	loadstats.wct.push_back(steady_clock::now());
	while(ht->load_factor() < p.loadfactor) {
		size_t k = data(rng);
		if (ht->insert(k, k/2))
			keys.push_back(k);
		loadstats.opcount++;
		if (--opscount == 0) {
			loadstats.wct.push_back(steady_clock::now());
			opscount = OPS_INTERVAL;
			cout << "\r[" << ht->load_factor()/p.loadfactor*100 
				<< "% complete]   " << flush;
		}
	}
	if (opscount != OPS_INTERVAL) loadstats.wct.push_back(steady_clock::now());

	cout << "\r[" << ht->load_factor()/p.loadfactor*100 << "% complete]     \n";
	cout << "\n======== Loading ========\n";
	cout << "\n\e[1mVerification\e[0m:\n"; ht->report_testing_stats();
	display_stats(loadstats, p.verbose);
   	dump_timing_data(loadstats);
	
	//
	// running phase
	// randomly insert, remove, or query, based on the specified proportions
	//
	stats_t runstats = { ht, {}, 0};
	ht->reset_perf_counts();
	opscount = OPS_INTERVAL;
	int x;

	cout << "\nBegin running phase\n"; 
	runstats.wct.push_back(steady_clock::now());
	int which, fail;
	for(size_t i=0; i<p.trials; ++i) {
		which = operation(rng);
		size_t k, index = 0;

		if (which < p.insert) {
			do {
				if (ht->table_size() == ht->num_records()) {
					cerr << "OOPS: table filled, trial " << i << "\n";
					break;
				}
				k = data(rng);
			} while	(!ht->insert(k, 42));
			keys.push_back(k);
		} else {
			// pick a key to query or remove
			if (!keys.empty()) {
				uniform_int_distribution<> pick(0,keys.size()-1);
				index = pick(rng);
				k = keys[index]; 
			} else {
				cerr << "ran out of keys\n";
				break;
			}

			if (which < p.insert + p.query) {
				fail = operation(rng);
				if (fail < p.fails) k = data(rng);
				ht->query(k,&x);
			} else { 
				if(ht->remove(k)) {
					swap(keys[index],keys.back());
					keys.pop_back();
				} 
			}
		}

		runstats.opcount++;
		if (--opscount == 0) {
			runstats.wct.push_back(steady_clock::now());
			opscount = OPS_INTERVAL;
			cout << "\r[" << 100*runstats.opcount/p.trials
				<< "% complete]" << flush;
		}
	}
	cout << "\r[" << 100*runstats.opcount/p.trials << "% complete]\n";
	if (opscount != OPS_INTERVAL) runstats.wct.push_back(steady_clock::now());

	cout << "\n======== Running ========\n\n";
	cout << "\e[1mVerification\e[0m:\n"; ht->report_testing_stats();
	display_stats(runstats, p.verbose);
	dump_timing_data(runstats);

	delete ht;
	return 0;
}

void
dump_timing_data(stats_t &stats, ostream &o)
{
	chrono::duration<double> t;
	int i;
	locale l = o.getloc();
	o.imbue(std::locale());
	
	for(i=1; i<stats.wct.size()-1; i++) {
		t = stats.wct[i] - stats.wct[i-1];
		o << OPS_INTERVAL*i << "," << t.count() << ","
			<< OPS_INTERVAL / t.count() << "\n"; 
	}
	t = stats.wct.back() - stats.wct[i-1];
	o << stats.opcount << "," << t.count() << ","
		<< (stats.opcount - OPS_INTERVAL*(i-1)) / t.count() << "\n";
	o.imbue(l);
}

void
display_stats(stats_t &stats, bool verbose)
{
	chrono::duration<double> time = stats.wct.back() - stats.wct.front();
	if (verbose) {
		verbose_stats(stats.ht);
		cout << "------ Timing statistics ------\n"
		     << "Performed " << stats.opcount << " operations\n"
		     << "Finished test in \e[35;1m" << time.count() << "\e[0m seconds"
			 << ", average ops/sec: " << stats.opcount / time.count()
			 << "\n" << endl;
	} else {
		terse_stats(stats.ht);
		cout << stats.opcount
			<< "," << time.count()
			<< "," << stats.opcount / time.count() << ",\n";
	}
}

void
terse_stats(hashtable *ht)
{
	cout << ht->table_size() << ',' << ht->num_records()
		<< ',' << ht->load_factor() << ',' << ht->total_misses
		<< ',' << ht->avg_misses() << ',' << ht->longest_search << ',';
}

void
verbose_stats(hashtable *ht)
{
	cout << "------ Table statistics ------\n";
	cout << "Buckets: " << ht->table_size() << "\n";
	cout << "Records: " << ht->num_records() << "\n";
	cout << "Load factor: " << ht->load_factor() << "\n";
	
	cout << "------ Search statistics ------\n";
	cout << "Total misses: " << ht->total_misses << "\n";
	cout << "Avg. miss/op: " << ht->avg_misses() << "\n";
	cout << "Longest search: " << ht->longest_search << "\n";
}

bool
setup_test_params(int argc, char **argv, params_t *p)
{
	p->insert = 33, p->query = 34, p->remove = 33;
	p->fails = 0;
	p->loadfactor = 0.7;
	p->type = params_t::none;
	p->table_size = 1'000'000;
	p->trials = 1'000'000;
	p->verbose = false;

	char *ptype = NULL;
	int c;
	while ((c = getopt(argc, argv, "t:i:q:f:b:l:n:v")) != -1) {
		switch(c) 
		{
		case 't':
			ptype = optarg;
		   	break;
		case 'f':
			p->fails = stoi(optarg);
			break;
		case 'i':
			p->insert = stoi(optarg);
			break;
		case 'q':
			p->query = stoi(optarg);
			break;
		case 'b':
			p->table_size = stoi(optarg)*1000;
			break;
		case 'l':		
			p->loadfactor = stod(optarg);
			if (!(p->loadfactor > 0 && p->loadfactor < 1)) {
				cout << "Load factor must be between 0 and 1\n";
				return false;
			}
			break;
		case 'n':
			p->trials = stoi(optarg)*1000;
			break;
		case 'v':
			p->verbose = true;
			break;
		case '?':
			if (optopt == 't' || optopt == 'i' || optopt == 'q' ||
				optopt == 'l' || optopt == 'b' || optopt == 'n' ||
				optopt == 'f')
				cout << "Option " << optopt << " takes an argument\n";
			else
				cout << "Unknown option supplied.\n"
					"Valid options are: t"
					"[addressing type: linear, quad, olinear, graveyard]\n"
					"i [insert %], q [query %],\n"
				    "f [fail %],\n" 
					"b [# of buckets], "
					"n [# of operations in thousands], l [load factor]\n"
					"v [verbose mode]\n";
			return false;
		default:
			abort();
		}
	}

	if (ptype == NULL) {
		cout << "Specify an addressing method.\n";
		return false;
	}

	switch(ptype[0]) {
		case 'l': p->type = params_t::linear; break;
		case 'q': p->type = params_t::quadratic; break;
		case 'o': p->type = params_t::ordered; break;
		case 'g': p->type = params_t::graveyard; break;
		default: cout << "Unrecognized addressing method.\n"; return false;
	}

	p->remove = 100 - p->insert - p->query;
	p->table_size = next_prime(p->table_size);

	return true;
}

