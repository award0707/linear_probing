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
#include "bench.h"
#include "primes.h"
#include "ordered.h"

#define KEY_MAX 2000000000L

int main(int argc, char **argv)
{
	using namespace std;
	params_t p;

	if(!setup_test_params(argc, argv, &p)) exit(1);

	//
	// generate the test dataset
	//
	pcg_extras::seed_seq_from<std::random_device> seed_source;
	pcg32 rng(seed_source);
	uniform_int_distribution<uint64_t> data(0,KEY_MAX);
	// normal_distribution<> data(KEY_MAX, 2);
	// x = round(data(rng));
	//

	hashtable *ht = new hashtable(p.table_size);
	ht->set_max_load_factor(1.0);	// disable automatic resizing

	if (p.verbose) {
		locale comma_locale(locale(), new comma_numpunct());
		cout.imbue(comma_locale);
		cout << "Using table type " << ht->table_type()
			<< ", table size=" << ht->table_size()
			<< " (" << (double)ht->table_size_bytes() << " bytes)"
			<< ", lf=" << p.load_factor << "\n\n";
	}

	//
	// set up filename for outputs
	//
	ostringstream oss;
	oss << ht->table_type()
		<< "-" << ht->table_size()
		<< "-" << p.load_factor;
	auto fnstr = oss.str();

	vector<int> keys;
	keys.reserve(p.table_size);

	// generate random numbers and insert into the table.
	// maintain a list of all keys inserted until target load factor reached
	int timing_interval = (p.table_size * p.load_factor) / p.intervals;
	stats_t loadstats;
	loadstats.ht = ht;
	loadstats.opcount = 0;
	loadstats.ops_interval = timing_interval;
	int stat_timer = timing_interval;

	cout << "Begin loading phase\n";
	push_timing_data(loadstats);
	uint64_t k;
	while(ht->load_factor() < p.load_factor) {
		k = data(rng);
		using result = hashtable::insert_result;
		result r = ht->insert(k, k/2);
		if (r == result::SUCCESS || r == result::NEEDS_REBUILD) {
			keys.push_back(k);
		}
		
		loadstats.opcount++;

		if (--stat_timer == 0) {
			push_timing_data(loadstats);
			ht->longest_search = 0;
			stat_timer = timing_interval;
			cout << "\r[" << ht->load_factor()/p.load_factor*100 
				<< "% complete]   " << flush;
		}
	}

	if (stat_timer != timing_interval)
		push_timing_data(loadstats);

	cout << "\r[" << ht->load_factor()/p.load_factor*100 << "% complete]     \n";
	cout << "\n======== Load complete ========\n";
	cout << "\n\e[1mVerification\e[0m:\n";
	ht->report_testing_stats();
	display_stats(loadstats, p.verbose);
	
	dump_timing_data(loadstats, cout);
	
	if (p.write_stats) {
		ofstream f("loading-" + fnstr + ".csv");
		//o << "Inserts, Search/insert, Queries, Search/query,"
		//	" Removes, Search/remove, Total, Search/op\n";
		//ht->report_testing_stats(f,false);
		display_stats(loadstats, false, f);
		dump_timing_data(loadstats, f);
		f.close();
	}
	
	delete ht;
	return 0;
}

void dump_cluster_len(std::vector<int> &h, std::ostream &o)
{
	o << "Size\n";
	for (auto it = h.begin(); it != h.end(); it++) 
		o << *it << "\n";
}

void push_timing_data(stats_t &s)
{
	s.wct.push_back(steady_clock::now());
	s.op.push_back(s.opcount);
	s.lf.push_back(s.ht->load_factor());
	s.inserts.push_back(s.ht->inserts);
	s.insert_misses.push_back(s.ht->insert_misses);
	s.longest_search.push_back(s.ht->longest_search);
}
	
void dump_timing_data(stats_t &stats, std::ostream &o)
{
	using std::setw;
	std::chrono::duration<double> t;
	std::size_t i;
	std::locale l = o.getloc();
	o.imbue(std::locale());

	const int w=16;

	std::ios fmt(nullptr);
	fmt.copyfmt(o);
	o << std::fixed << std::left << std::setprecision(5)
	  << setw(w) << "Operations"
	  << setw(w) << "Time"
	  << setw(w) << "Ops_per_sec"
	  << setw(w) << "Load_factor"
	  << setw(w) << "Miss_per_insert"
	  << setw(w) << "Longest_search"
	  << "\n";
	for(i=1; i<stats.wct.size(); i++) {
		t = stats.wct[i] - stats.wct[i-1];
		uint64_t ops = stats.op[i] - stats.op[i-1];
		uint64_t ins = stats.inserts[i] - stats.inserts[i-1];
		uint64_t ins_m =
			stats.insert_misses[i] - stats.insert_misses[i-1];

		o << setw(w) << stats.op[i]
		  << setw(w) << std::setprecision(5) << t.count()
		  << setw(w) << std::setprecision(1) << ops / t.count()
		  << setw(w) << std::setprecision(4) << stats.lf[i]
		  << setw(w) << std::setprecision(4) << (double)ins_m / ins
		  << setw(w) << stats.longest_search[i]
		  << "\n"; 
	}

	t = stats.wct.back() - stats.wct.front();
	o << '\n';
	o << setw(w) << "Total ops"
		<< setw(w) << "Total Time"
		<< setw(w) << "Ops_per_sec" << '\n';
	o << setw(w) << stats.opcount
		<< setw(w) << t.count()
		<< setw(w) << stats.opcount / t.count() << "\n";

	o.copyfmt(fmt);
	o.imbue(l);
}

void display_stats(stats_t &stats, bool verbose, std::ostream &o)
{
	using std::setw;
	std::chrono::duration<double> time = stats.wct.back() - stats.wct.front();
	if (verbose) {
		verbose_stats(stats.ht);
		o << "------ Timing statistics ------\n"
		     << "Performed " << stats.opcount << " operations\n"
		     << "Finished test in \e[35;1m" << time.count() << "\e[0m seconds"
			 << ", average ops/sec: " << stats.opcount / time.count()
			 << "\n" << std::endl;
	} else {
		int w=16;
		o << std::left
			<< setw(w) << "size"
			<< setw(w) << "records"
			<< setw(w) << "lf"
			<< setw(w) << "misses"
			<< setw(w) << "avg misses"
			<< setw(w) << "longest" << '\n';
		o << setw(w) << stats.ht->table_size() 
			<< setw(w) << stats.ht->num_records()
			<< setw(w) << stats.ht->load_factor()
			<< setw(w) << stats.ht->total_misses
			<< setw(w) << stats.ht->avg_misses()
			<< setw(w) << stats.ht->longest_search << '\n';
	}
	o << '\n';
}

void verbose_stats(hashtable *ht)
{
	std::cout << "------ Table statistics ------\n";
	std::cout << "Buckets: " << ht->table_size() << "\n";
	std::cout << "Records: " << ht->num_records() << "\n";
	std::cout << "Load factor: " << ht->load_factor() << "\n";
	
	std::cout << "------ Search statistics ------\n";
	std::cout << "Total misses: " << ht->total_misses << "\n";
	std::cout << "Avg. miss/op: " << ht->avg_misses() << "\n";
	std::cout << "Longest search: " << ht->longest_search << "\n";
}

bool
setup_test_params(int argc, char **argv, params_t *p)
{
	p->fails = 0;
	p->load_factor = 0.7;
	p->table_size = 1'000'000;
	p->trials = 1'000'000;
	p->intervals = 100;
	p->verbose = false;
	p->write_timing_data = false;
	p->write_cluster_len = false;
	p->write_stats = false;

	int c;
	while ((c = getopt(argc, argv, "f:b:l:n:i:vTCS")) != -1) {
		switch(c) 
		{
		case 'f':
			p->fails = std::stoi(optarg);
			break;
		case 'b':
			p->table_size = std::stoi(optarg)*1000000;
			break;
		case 'l':		
			p->load_factor = std::stod(optarg);
			if (!(p->load_factor > 0 && p->load_factor < 1)) {
				std::cout << "Load factor must be between 0 and 1\n";
				return false;
			}
			break;
		case 'n':
			p->trials = std::stoi(optarg)*1000000;
			break;
		case 'i':
			p->intervals = std::stoi(optarg);
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
				std::cout << "Option " << optopt << " takes an argument\n";
			else
				std::cout << "Unknown option supplied.\n"
					"f [fail %],\n" 
					"b [# of buckets in millions], "
					"n [# of operations in millions], l [load factor]\n"
					"i [# of timing intervals]\n"
					"v [verbose mode]\n"
					"T [detailed timing data], C [cluster size data], "
					"S [table statistics]\n";
			return false;
		default:
			abort();
		}
	}

	p->table_size = next_prime(p->table_size);

	return true;
}

