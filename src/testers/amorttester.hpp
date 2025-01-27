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
#include "util.h"

using std::chrono::duration;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::uniform_int_distribution;
using std::cout, std::vector;

template <typename hashtable>
class amorttester {
	private:
	pcg64 &rng;
	std::string type;
	int x, nops, ntests;
	uint64_t b;

	struct amort_stats_t {
		uint64_t b;
		int x;
		int nops;
		std::vector<duration<double>> ops_time;
		double mean_ops_time;
		double median_ops_time;
		int rw;
		double alpha;
		std::size_t n;
	};
	std::vector<amort_stats_t> stats;

	inline void
	loadrebuild(hashtable *ht)
	{
		// specialized function at the bottom of this file
		// because rebuilding really speeds up loading for graveyards
	}

	// make a set of keys for loading and floating ops, no duplicates
	void
	gen_testset(std::vector<uint32_t>* loadset, uint32_t n)
	{
		cout << "Generate test set\n";
		n += nops * ntests;
		loadset->reserve(n);
		selsample(loadset,n,std::numeric_limits<uint32_t>::max(),rng);

		cout << "Loadset generated\n";
		std::shuffle(std::begin(*loadset), std::end(*loadset), rng);
		cout << "Loadset shuffled, size = " << loadset->size() << "\n";
	}

	// generate random numbers and insert into the table.
	// maintain list of all keys inserted until target load factor reached
	void
	loadtable(hashtable *ht, std::vector<uint32_t> *loadset,
	          std::vector<uint32_t> *inserted, double lf)
	{
		int loadops = ht->table_size() * lf;
		int interval = loadops / 20;
		int stat_timer = interval;
		uint64_t k;
		int start = 0;

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

	void floating(hashtable *ht,
	              std::vector<uint32_t> *testset,
	              std::vector<uint32_t> *inserted,
		      std::vector<uint32_t> *delorder,
	              const std::vector<uint8_t> &opset)
	{
		uint32_t k;
		using res = hashtable::result;
		res r;
		int x=0,y=0, fr=0;

		for (int i=0; i<nops; ++i) {
			if (opset[i] == 1) {
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
				assert(!delorder->empty());
				k = (*inserted)[delorder->back()];
				r = ht->remove(k);
				if (r != res::FAILURE) {
					(*inserted)[delorder->back()] = inserted->back();
					inserted->pop_back();
					delorder->pop_back();
				}
				else { std::cerr << "What?\n"; fr++; }
				++y;
			}

			if (r == res::REBUILD) {
				uint32_t br=ht->num_records();
				ht->rebuild();
				std::cerr << "R" << ht->rebuilds << ": N="<<ht->table_size() << ",n="<<ht->num_records()<<",br="<<br<<",i="<<i<<",op="<<(int)opset[i]<<"\n";
				if (ht->num_records() != br) {
					std::cerr << "the rebuild took place after inserting k="<<k<<"\n";
					assert(ht->num_records() == br);
				}
			}
		}
		std::cerr << "x=" << x << ",y="<< y<<'\n';
	}

	void float_timer(hashtable *ht,
                         std::vector<uint32_t> *testset,
	                 std::vector<uint32_t> *inserted,
	                 std::vector<duration<double>> *optimes)
	{
		time_point<steady_clock> start, end;
		ht->rebuild(); 
		ht->rebuilds = 0;

		ht->reset_perf_counts();
		cout << "timing floating operations with rebuilds: ";

		for (int i=0; i<ntests; ++i) {
			cout << i+1 << std::flush;
			// operations 
			std::vector<uint8_t> opset;
			opset.reserve(nops);
			for (int j = (i&1); j < nops; ++j) opset.push_back(j & 1);
			//std::shuffle(std::begin(opset), std::end(opset), rng);

			// deletion order
			std::vector<uint32_t> delorder;
			delorder.reserve(nops/2+1);
			std::uniform_int_distribution<> U(0,inserted->size()/2);
			while((int)delorder.size() < nops/2+1) 
				delorder.push_back(U(rng));
			cout << ".." << std::flush;

			// timed section - floating ops and rebuild
			start = steady_clock::now();
			floating(ht, testset, inserted, &delorder, opset);
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
		std::vector <uint32_t> testset;
		std::vector <uint32_t> inserted;
		hashtable ht(next_prime(b));
		type = ht.table_type();
		cout << type << "\n";

		gen_testset(&testset, ht.table_size());

		ht.set_max_load_factor(1.0);

		vector <duration<double>> op_times;
		double lf = 1.0 - (1.0 / x);

		cout << ht.table_type() << " "
		     << ht.table_size() << ", x="
		     << x << std::endl;

		loadtable(&ht, &testset, &inserted, lf);
		float_timer(&ht, &testset, &inserted, &op_times);

		amort_stats_t q {
			.b                   = b,
			.x                   = x,
			.nops                = nops,
			.ops_time            = op_times,
			.mean_ops_time       = mean(op_times),
			.median_ops_time     = median(op_times),
			.rw                  = ht.get_rebuild_window(),
			.alpha               = ht.load_factor(),
			.n                   = ht.table_size(),
		};

		stats.push_back(q);
	}

	public:
	amorttester(pcg64 &r, int x, uint64_t b, int no, int nt)
	             : rng(r), x(x), nops(no), ntests(nt), b(b) {
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
