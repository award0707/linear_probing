#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <random>

#include "pcg_random.hpp"

#include "primes.h"
#include "bench.h"
#include "linear.h"
#include "quadratic.h"
#include "ordered.h"
#include "graveyard.h"

using namespace std;

#define KEY_MAX 2000000000L

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
	// set up filename for outputs
	//
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	ostringstream oss;
	oss << ht->table_type()
		<< "-i" << p.insert << "_q" << p.query << "_r" << p.remove
		<< "-f" << p.fails
		<< "-s" << ht->table_size()
		<< "-l" << p.loadfactor
		<< std::put_time(&tm, "_%d-%m-%Y-%H-%M-%S");
	auto fnstr = oss.str();
	ofstream f;

	//
	// generate the test dataset
	//
	pcg_extras::seed_seq_from<std::random_device> seed_source;
	pcg32 rng(seed_source);
	uniform_int_distribution<int> data(0,KEY_MAX);
	// normal_distribution<> data(KEY_MAX, 2);
	// x = round(data(rng));

	vector<int> keys;

	//
	// initialization phase
	// generate random numbers and insert into the table.
	// maintain a list of all keys inserted until target load factor reached
	//
	stats_t loadstats = { ht, {}, 0, p.ops_interval };
	int opscount = p.ops_interval;

	cout << "Begin loading phase\n";
	loadstats.wct.push_back(steady_clock::now());
	int loadindex = 0;
	size_t k = 0;
	while(ht->load_factor() < p.loadfactor) {
		k = data(rng);
		if (ht->insert(k, k/2))
			keys.push_back(k);
		
		loadstats.opcount++;
		if (--opscount == 0) {
			loadstats.wct.push_back(steady_clock::now());
			opscount = p.ops_interval;
			cout << "\r[" << ht->load_factor()/p.loadfactor*100 
				<< "% complete]   " << flush;
		}
	}
	if (opscount != p.ops_interval)
		loadstats.wct.push_back(steady_clock::now());

	cout << "\r[" << ht->load_factor()/p.loadfactor*100 << "% complete]     \n";
	cout << "\n======== Load complete ========\n";
	cout << "\n\e[1mVerification\e[0m:\n";
	ht->report_testing_stats();
	display_stats(loadstats, p.verbose);
	
	if (p.write_stats) {
		f.open("loadstats-" + fnstr + ".csv");
		f << "Inserts, Search/insert, Queries, Search/query,"
			" Removes, Search/remove, Total, Search/op\n";
		ht->report_testing_stats(f,false);
		f.close();
	}

	//
	// running phase
	// randomly insert, remove, or query, based on the specified proportions
	//
	uniform_int_distribution<mt19937::result_type> operation(0,99); 
	stats_t runstats = { ht, {}, 0, p.ops_interval };
	ht->reset_perf_counts();
	opscount = p.ops_interval;

	cout << "\nBegin running phase\n"; 
	runstats.wct.push_back(steady_clock::now());
	int which, fail;
	for(size_t i=0; i<p.trials; ++i) {
		which = operation(rng);
		int x;
		size_t k, index = 0;

		if (which < p.insert) {
			if (ht->table_size() == ht->num_records()) {
				cerr << "OOPS: table filled, trial " << i << "\n";
				break;
			}
			do 
				k = data(rng);
			while (!ht->insert(k, 42)); 
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
				fail = operation(rng);	// randomly fail queries
				if (fail < p.fails) k = data(rng);
				ht->query(k,&x);
			} else { 
				if(ht->remove(k)) {
					swap(keys[index], keys.back());
					keys.pop_back();
				}
			}
		}
		
		runstats.opcount++;
		if (--opscount == 0) {
			runstats.wct.push_back(steady_clock::now());
			opscount = p.ops_interval;
			cout << "\r[" << 100*runstats.opcount/p.trials
				<< "% complete]" << flush;
		}
	}
	cout << "\r[" << 100*runstats.opcount/p.trials << "% complete]\n";
	if (opscount != p.ops_interval) runstats.wct.push_back(steady_clock::now());

	cout << "\n======== Run complete ========\n\n";
	cout << "\e[1mVerification\e[0m:\n";
	ht->report_testing_stats();
	display_stats(runstats, p.verbose);

	if (p.write_stats) {
		f.open("runstats-" + fnstr + ".csv");
		f << "Inserts, Search/insert, Queries, Search/query,"
			" Removes, Search/remove, Total, Search/op\n";
		ht->report_testing_stats(f,false);
		f.close();
	}

	if (p.write_timing_data) {
		f.open("loadperf-" + fnstr + ".csv");
		dump_timing_data(loadstats,f);
		f.close();
		
		f.open("runperf-" + fnstr + ".csv");
		dump_timing_data(runstats,f);
		f.close();
	}	

	if (p.write_cluster_len) {
		vector<int> empty, tomb;
		ht->cluster_len(empty, tomb);

		f.open("clempty-" + fnstr + ".csv");
		dump_cluster_len(empty, f);
		f.close();

		f.open("cltomb-" + fnstr + ".csv");
		dump_cluster_len(tomb, f);
		f.close();
	}

	delete ht;
	return 0;
}

void
dump_cluster_len(vector<int> &h, ostream &o)
{
	o << "Size\n";
	for (auto it = h.begin(); it != h.end(); it++) 
		o << *it << "\n";
}

void
dump_timing_data(stats_t &stats, ostream &o)
{
	chrono::duration<double> t;
	int i;
	locale l = o.getloc();
	o.imbue(std::locale());

	o << "Operations, Time, Ops/sec\n";
	for(i=1; i<stats.wct.size()-1; i++) {
		t = stats.wct[i] - stats.wct[i-1];
		o << stats.ops_interval*i << "," << t.count() << ","
			<< stats.ops_interval / t.count() << "\n"; 
	}
	t = stats.wct.back() - stats.wct[i-1];
	o << stats.opcount << "," << t.count() << ","
		<< (stats.opcount - stats.ops_interval*(i-1)) / t.count() << "\n";
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
	p->ops_interval = 1'000'000;
	p->verbose = false;
	p->write_timing_data = false;
	p->write_cluster_len = false;
	p->write_stats = false;

	char *ptype = NULL;
	int c;
	while ((c = getopt(argc, argv, "t:i:q:f:b:l:n:o:vTCS")) != -1) {
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
		case 'o':
			p->ops_interval = stoi(optarg)*1000;
			break;
		case 'v':
			p->verbose = true;
			break;
		case 'T':
			p->write_timing_data = true;
			break;
		case 'C':
			p->write_cluster_len = true;
			break;
		case 'S':
			p->write_stats = true;
			break;
		case '?':
			if (optopt == 't' || optopt == 'i' || optopt == 'q' ||
				optopt == 'l' || optopt == 'b' || optopt == 'n' ||
				optopt == 'f' || optopt == 'o')
				cout << "Option " << optopt << " takes an argument\n";
			else
				cout << "Unknown option supplied.\n"
					"Valid options are: t"
					"[addressing type: linear, quad, olinear, graveyard]\n"
					"i [insert %], q [query %],\n"
				    "f [fail %],\n" 
					"b [# of buckets], "
					"n [# of operations in thousands], l [load factor]\n"
					"o [timing data interval in thousands]\n"
					"v [verbose mode]\n"
					"T [detailed timing data], C [cluster size data], "
					"S [table statistics]\n";
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

