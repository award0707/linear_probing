#ifndef AMORTTESTER_HPP
#define AMORTTESTER_HPP

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <numeric>
#include <random>
#include <chrono>

#include "pcg_random.hpp"
#include "primes.h"
#include "graveyard.h"

using std::chrono::duration;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::uniform_int_distribution;
using std::cout, std::vector;

template <typename hashtable>
class amorttester {
	private:
	std::string type;
	pcg64 &rng;
	const std::vector<int> &xs;
	const std::vector<uint64_t> &bs;
	int nops;
	int ntests;

	struct amort_stats_t {
		int nops;
		std::vector<duration<double>> ops_time;
		double mean_ops_time;
		double median_ops_time;
		int rw;
		double alpha;
		int x;
		std::size_t n;
	};
	std::vector<amort_stats_t> stats;

	inline void
	loadrebuild(hashtable *ht)
	{
		// specialized function at the bottom of this file
		// because rebuilding really speeds up loading for graveyards
	}

	// TODO: overhaul this function to make it much more efficient.
	// The "make a list and shuffle it" needs 16GB ram and takes forever.
	// https://bastian.rieck.me/blog/2017/selection_sampling/

	// make a set of keys for loading and floating ops, no duplicates
	void
	gen_testset(std::vector<uint32_t>* loadset, uint32_t n,
	            std::vector<uint32_t>* testset,
	            std::vector<uint8_t>* opset)
	{
		cout << "Generate test set\n";
		loadset->reserve(UINT32_MAX);
		for (uint32_t i = 0; i< UINT32_MAX; ++i)
			loadset->push_back(i);

		cout << "Loadset generated\n";
		std::shuffle(std::begin(*loadset), std::end(*loadset), rng);

		cout << "Loadset shuffled\n";

		uint32_t idx = (nops/2) * ntests * xs.size();
		testset->reserve(idx);
		testset->insert(testset->end(),
		                std::make_move_iterator(loadset->end() - idx),
		                std::make_move_iterator(loadset->end()));

		loadset->resize(n);
		loadset->shrink_to_fit();

		opset->reserve(nops);
		for (int i = 0; i < nops; ++i)
			opset->push_back(i & 1);
		//std::shuffle(std::begin(*opset), std::end(*opset), rng);

		cout << "loadset=" << loadset->size() << ", testset="
		     << testset->size() << ", opset=" << opset->size() << '\n';
	}

	// generate random numbers and insert into the table.
	// maintain list of all keys inserted until target load factor reached
	void
	loadtable(hashtable *ht, std::vector<uint32_t> *loadset,
	          std::vector<uint32_t> *inserted, double lf)
	{
		double start = ht->load_factor();
		int loadops = ht->table_size() * (lf - start);
		int interval = loadops / 20;
		int stat_timer = interval;
		uint64_t k;

		cout << "Load: " << start << " -> " << lf << "\n";

		for (int i = 0; i < loadops; ++i) {
			k = loadset->back();
			loadset->pop_back();
			using result = hashtable::result;
			result r = ht->insert(k, k>>2);
			switch(r) {
			case result::SUCCESS:
				inserted->push_back(k);
				break;
			case result::REBUILD:
				inserted->push_back(k);
				loadrebuild(ht);
				break;
			case result::FULLTABLE: // this should never happen
				std::cerr << "Table full!\n";
				return;
			case result::DUPLICATE: // this also shouldn't happen
				std::cerr << "Duplicate found\n";
			default:
				break;
			}

			if (--stat_timer == 0) {
				stat_timer = interval;
				cout << "\r["
				     << 100 * (ht->load_factor() - start)
				        / (lf - start)
				     << "%]   " << std::flush;
			}
		}

		cout << "\r[done]     , inserted=" << inserted->size() << "\n";
	}

	// TODO: even with alternating operations, the table fills up even if
	// operations are alternating. This bug MUST be fixed, or load factor cannot
	// be controlled.

	// TODO: with alternating operations, this code always pops the last
	// element that was pushed. thus an artificial locality between each
	// insert and delete exists, leading to false results. This bug MUST be fixed.
	// fix: create a preset delete order in the gen_test function; delete from
	//      loadset until testset has enough items in it.

	void floating(hashtable *ht, std::vector<uint32_t> *testset,
	              std::vector<uint32_t> *inserted, std::vector<uint8_t> *opset)
	{
		uint32_t k;
		using res = hashtable::result;
		res r;
		int x=0,y=0;

		for (int i=0; i<nops; ++i) {
			if ((*opset)[i]) {
				assert(ht->num_records() < ht->table_size());
				assert(!testset->empty());
				k = testset->back();
				r = ht->insert(k, 42);
				if (r == res::SUCCESS || r == res::REBUILD) {
					testset->pop_back();
					inserted->push_back(k);
				} else
					std::cerr << "Duplicate insert!\n";
				++x;
			} else {
				assert(!inserted->empty());
				k = inserted->back();
				r = ht->remove(k);
				if (r != res::FAILURE) inserted->pop_back();
				else std::cerr << "What?\n";
				++y;
			}

			if (r == res::REBUILD) {
				ht->rebuild();
				std::cerr << "R" << ht->rebuilds << ": N="<<ht->table_size() << ",n="<<ht->num_records()<<'\n';
			}
		}
		std::cerr << "x=" << x << ",y="<< y<<'\n';
	}

	void float_timer(hashtable *ht, std::vector<uint32_t> *testset,
	           std::vector<uint32_t> *inserted,
	           std::vector<uint8_t> *opset,
	           std::vector<duration<double>> *optimes)
	{
		time_point<steady_clock> start, end;
		ht->rebuild();  // start from a "good" state
		ht->rebuilds = 0;
		ht->reset_perf_counts();
		cout << "timing floating operations with rebuilds: ";
		for (int i=0; i<ntests; ++i) {
			cout << i+1 << std::flush;
			std::shuffle(std::begin(*opset), std::end(*opset), rng);
			cout << ".." << std::flush;

			// timed section - floating ops and rebuild
			start = steady_clock::now();
			floating(ht, testset, inserted, opset);
			end = steady_clock::now();
			optimes->push_back(end - start);

			cout << "(" << ht->rebuilds << ").. " << std::flush;

			ht->rebuild();
			ht->rebuilds = 0;
			ht->reset_perf_counts();
		}
		cout << std::endl;
	}

	std::ostream& dump_amort_stats(std::ostream &o = std::cout) const
	{
		o << "\n----- " << type
		     << " --------------------------------\n";
		o << "# ops, times, mean, median, RW, a, x, n\n";

		for (amort_stats_t q : stats) {
			o << q.nops << ", "
		//	  << q.ops_time << ", "
			  << q.mean_ops_time << ", "
			  << q.median_ops_time << ", "
			  << q.rw << ", "
			  << q.alpha << ", "
			  << q.x << ", "
			  << q.n << '\n';
		}

		return o;
	}

	void run_test()
	{
		for (auto b : bs) {
			std::vector <uint32_t> loadset, testset;
			std::vector <uint8_t> opset;
			std::vector <uint32_t> inserted;
			hashtable ht(next_prime(b));
			type = ht.table_type();
			cout << type << "\n";

			gen_testset(&loadset, ht.table_size(), &testset, &opset);

			ht.set_max_load_factor(1.0);
			for (auto x : xs) {
				vector <duration<double>> op_times;
				double lf = 1.0 - (1.0 / x);

				cout << ht.table_type() << " "
				     << ht.table_size() << ", x="
				     << x << std::endl;

				loadtable(&ht, &loadset, &inserted, lf);

				float_timer(&ht, &testset, &inserted, &opset, 
				            &op_times);

				amort_stats_t q {
					.nops                = nops,
					.ops_time            = op_times,
					.mean_ops_time       = mean(op_times),
					.median_ops_time     = median(op_times),
					.rw                  = ht.get_rebuild_window(),
					.alpha               = ht.load_factor(),
					.x                   = x,
					.n                   = ht.table_size(),
				};

				stats.push_back(q);
			}
		}
	}

	public:
	amorttester(pcg64 &r, std::vector<int> const &x,
	            std::vector<uint64_t> const &b, int no, int nt)
	             : rng(r), xs(x), bs(b), nops(no), ntests(nt) {
		run_test();
	}

	friend std::ostream&
	operator<<(std::ostream& os, amorttester const& h) {
		return h.dump_amort_stats(os);
	}
};

template<> inline void
amorttester<graveyard_aos<>>::loadrebuild(graveyard_aos<> *ht)
{
	ht->rebuild();
}

template<> inline void
amorttester<graveyard_soa<>>::loadrebuild(graveyard_soa<> *ht)
{
	ht->rebuild();
}

#endif
