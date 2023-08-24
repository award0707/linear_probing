#ifndef LOADTESTER_HPP
#define LOADTESTER_HPP

#include <iostream>
#include <iomanip>
#include <chrono>
#include <unistd.h>
#include <vector>
#include <map>
#include <random>

#include "pcg_random.hpp"
#include "primes.h"

//#define VERIFY    /* debug: exhaustively test all keys and values inserted */
//#define VERBOSE   /* enable progress meter */

using std::chrono::steady_clock;
using std::chrono::time_point;

template <typename hashtable>
class loadtester {
	private:
	pcg64 &rng;
	hashtable ht;
	std::size_t opcount;
	double target_lf;
	int intervals;
	bool loadrebuild;

	struct stats_t {
		// record stats at the end of each interval
		std::vector<time_point<steady_clock>> wct; // time
		std::vector<std::size_t> op; // opcount
		std::vector<double> lf;	// load factor
		std::vector<uint64_t> ins;              // inserts
		std::vector<uint64_t> ins_misses;       // misses
		std::vector<uint64_t> ins_shifts;       // shift distances
		std::vector<uint64_t> longest_search;
	} stats;

	void push_timing_data()
	{
		stats.wct.push_back(steady_clock::now());
		stats.op.push_back(opcount);
		stats.lf.push_back(ht.load_factor());
		stats.ins.push_back(ht.inserts);
		stats.ins_misses.push_back(ht.insert_misses);
		stats.ins_shifts.push_back(ht.insert_shifts);
		stats.longest_search.push_back(ht.longest_search);
	}

	std::ostream& dump_timing_data(std::ostream &o) const
	{
		using std::setw;
		std::chrono::duration<double> t;
		std::size_t i;

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
		  << setw(w) << "Shift_per_insert"
		  << "\n";

		for (i=1; i<stats.wct.size(); i++) {
			uint64_t ops, ins, ins_m, ins_s;
			t = stats.wct[i] - stats.wct[i-1];
			ops = stats.op[i] - stats.op[i-1];
			ins = stats.ins[i] - stats.ins[i-1];
			ins_m = stats.ins_misses[i] - stats.ins_misses[i-1];
			ins_s = stats.ins_shifts[i] - stats.ins_shifts[i-1];

			o << setw(w) << stats.op[i]
			  << setw(w) << std::setprecision(5) << t.count()
			  << setw(w) << std::setprecision(1) << ops / t.count()
			  << setw(w) << std::setprecision(4) << stats.lf[i]
			  << setw(w) << std::setprecision(4)
			  << (double)ins_m / ins
			  << setw(w) << stats.longest_search[i]
			  << setw(w) << std::setprecision(4)
			  << (double)ins_s / ins
			  << "\n";
		}

		t = stats.wct.back() - stats.wct.front();
		o << setw(w) << "Total ops"
			<< setw(w) << "Total Time"
			<< setw(w) << "Ops_per_sec" << '\n';
		o << setw(w) << opcount
			<< setw(w) << t.count()
			<< setw(w) << opcount / t.count() << "\n";

		return o;
	}

	std::ostream& table_info(std::ostream &o) const
	{
		using std::setw;
		std::chrono::duration<double> time = stats.wct.back()
		                                     - stats.wct.front();
		const int w=16;
		o << std::left
		  << setw(w) << "type"
		  << setw(w) << "size"
		  << setw(w) << "records"
		  << setw(w) << "lf"
		  << setw(w) << "misses"
		  << setw(w) << "avg misses"
		  << setw(w) << "longest"
		  << setw(w) << "shifts" << '\n'
		  << setw(w) << ht.table_type()
		  << setw(w) << ht.table_size()
		  << setw(w) << ht.num_records()
		  << setw(w) << ht.load_factor()
		  << setw(w) << ht.total_misses
		  << setw(w) << ht.avg_misses()
		  << setw(w) << ht.longest_search
		  << setw(w) << ht.insert_shifts << '\n';

		return o;
	}


	void run_test()
	{
		ht.set_max_load_factor(1.0);	// disable automatic resizing

		std::cout << "Table type " << ht.table_type()
		          << ", n=" << ht.table_size()
		          << " (" << (double)ht.table_size_bytes() << " bytes)"
		          << ", lf=" << target_lf << "\n";

		const std::size_t max = std::numeric_limits<uint32_t>::max();
		std::uniform_int_distribution<uint32_t> data(0,max);
		std::vector<uint32_t> keys;
		keys.reserve(ht.table_size());

		int insert_interval = (ht.table_size() * target_lf) / intervals;
		opcount = 0;

		uint32_t k;
		int stat_timer = insert_interval;
		push_timing_data();
		while(ht.load_factor() < target_lf) {
			k = data(rng);
			using result = hashtable::result;
			result r = ht.insert(k, k>>2);
		//	if (opcount == 10000) {
		//		std::cout << "put in " << k << ", " << k>>2
		//			<< ": 

			if (r == result::SUCCESS || r == result::REBUILD)
				keys.push_back(k);
			if (r == result::REBUILD && loadrebuild)
				ht.rebuild();

			opcount++;
			if (--stat_timer == 0) {
				push_timing_data();
				ht.longest_search = 0;
				stat_timer = insert_interval;
#ifdef VERBOSE
				std::cout << "\r["
				          << ht.load_factor()/target_lf*100
				          << "% complete]   " << std::flush;
#endif
			}
		}

		if (stat_timer != insert_interval)
			push_timing_data();
#ifdef VERBOSE
		std::cout << "\r[" << ht.load_factor()/target_lf*100
		          << "% complete]     \n";
		std::cout << "\n======== Load complete ========\n";
#endif

#ifdef VERIFY
		std::cout << "\n\e[1mVerification\e[0m:\n";
		uint32_t v;
		for(uint32_t x : keys) {
			assert(ht.query(x,&v) == true);
			assert(v == x>>2);
		}
#endif
		ht.report_testing_stats();
	}

	public:
	loadtester(pcg64 &r, size_t n, int x, int i, bool lr)
	         : rng(r), ht(next_prime(n)), 
	           intervals(i), loadrebuild(lr)
	{
		target_lf = 1 - 1.0/x;
		run_test();
	}

	void cluster_length(std::ostream &o)
	{
		std::map<int,int> m;
		ht.cluster_len(&m);
		// [cluster length]:[cardinality]
		for (auto pair : m)
			o << pair.first << ':' << pair.second << ", ";
	}

	void shift_distance(std::ostream &o)
	{
		std::map<int,int> m;
		ht.shift_distance(&m);
		// [shift distance]:[cardinality]
		for (auto pair : m)
			o << pair.first << ':' << pair.second << ", ";

	}

	friend std::ostream&
	operator<<(std::ostream& os, loadtester const& h) {
		return h.dump_timing_data(h.table_info(os));
	}
};

#endif

