#ifndef BENCH_H
#define BENCH_H

#include <vector>
#include <ostream>
#include <string>
#include "hashtable.h"

#define OPS_INTERVAL 1'000'000

using std::chrono::steady_clock;
using std::chrono::time_point;

struct stats_t {
	hashtable *ht;
	std::vector<time_point<steady_clock> > wct;
	std::size_t opcount;
};

struct params_t {
	int insert, query, remove;
	int fails;
	double loadfactor;
	size_t table_size, trials;
	bool verbose;
	enum { none, linear, quadratic, ordered, graveyard } type;
};

// for readable comma-separated prints in verbose mode
class comma_numpunct : public std::numpunct<char> { 
	protected:
		virtual char do_thousands_sep() const { return ','; }
		virtual std::string do_grouping() const { return "\03"; }
};

bool setup_test_params(int argc, char **argv, params_t *p);
void display_stats(stats_t &stats, bool verbose);
void dump_timing_data(stats_t &stats, std::ostream &o = std::cout);
void verbose_stats(hashtable *ht);
void terse_stats(hashtable *ht);

#endif
