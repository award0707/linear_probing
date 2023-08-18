#ifndef BENCH_H
#define BENCH_H

#include <vector>
#include <ostream>
#include <string>
#include <map>
#include "linear.h"

using std::chrono::steady_clock;
using std::chrono::time_point;

using hashtable = linear_aos;

struct stats_t {
	hashtable *ht;
	// record stats at the end of each interval
	std::vector<time_point<steady_clock> > wct; // time
	std::vector<std::size_t> op; // opcount
	std::vector<double> lf;	// load factor
	std::vector<uint64_t> inserts;
	std::vector<uint64_t> insert_misses;
	std::vector<uint64_t> longest_search;
	std::size_t opcount;
	int ops_interval;
};

struct params_t {
	int fails;
	double load_factor;
	size_t table_size, trials;
	bool verbose;
	bool write_stats;
	bool write_timing_data;
	bool write_cluster_len;
	int intervals;
	int ops_interval;
};

// for readable comma-separated prints in verbose mode
class comma_numpunct : public std::numpunct<char> { 
	protected:
		virtual char do_thousands_sep() const { return ','; }
		virtual std::string do_grouping() const { return "\03"; }
};

bool setup_test_params(int argc, char **argv, params_t *p);
void display_stats(stats_t &stats, bool verbose, std::ostream &o = std::cout);
void dump_cluster_len(std::vector<int> &h, std::ostream &o = std::cout);
void push_timing_data(stats_t &s);
void dump_timing_data(stats_t &stats, std::ostream &o = std::cout);
void verbose_stats(hashtable *ht);

#endif
