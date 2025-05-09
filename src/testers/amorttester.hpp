#ifndef AMORTTESTER_HPP
#define AMORTTESTER_HPP

#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <numeric>
#include <random>
#include <chrono>
//#define NDEBUG
#include <cassert>

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
	double x;
	int nops, ntests;
	uint64_t b;

	struct amort_stats_t {
		uint64_t b;     // desired # of slots
		double x;       // load ratio: 1/x slots are empty
		int nops;       // ops per test
		std::vector<duration<double>> ops_time;
		double mean_ops_time;
		double median_ops_time;
		double total_ops_time;
		double mean_ins_time;
		double total_ins_time;
		double mean_rb_time;
		double total_rb_time;
		unsigned int rw;        // rebuild window
		unsigned int rb;        // number of rebuilds
		double alpha;           // actual load factor after test
		std::size_t n;          // actual size (smallest prime >= b)
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
		n += nops * ntests;
		cout << "Reserve testset, size " << n << "..." << std::flush; 
		loadset->reserve(n);
		cout << "done. Generate testset..." << std::flush; 
		selsample(loadset,n,std::numeric_limits<uint32_t>::max(),rng);

		cout << "done. Shuffling..." << std::flush;
		std::shuffle(std::begin(*loadset), std::end(*loadset), rng);
		cout << "done, size = " << loadset->size() << "\n";
	}

	// generate random numbers and insert into the table.
	// maintain list of all keys inserted until target load factor reached
	void
	loadtable(hashtable *ht, std::vector<uint32_t> *loadset,
	          std::vector<uint32_t> *inserted, double lf)
	{
		using result = hashtable::result;
		int loadops = ht->table_size() * lf;
		int interval = loadops / 20;
		int stat_timer = interval;
		uint64_t k;
		int start = 0;

		cout << "Load: " << start << " -> " << lf << "\n";

		for (int i = 0; i < loadops; ++i) {
			k = loadset->back();
			loadset->pop_back();
			result r = ht->insert(k, k);
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
	              const std::vector<uint8_t> &opset,
		      std::vector<duration<double>> *insert_times,
		      std::vector<duration<double>> *rebuild_times)
	{
		enum hashtable::result r;

		uint32_t k;
		time_point<steady_clock> t1, t2;

		duration<double> insert_time(0), rebuild_time(0);

		t1 = steady_clock::now();
		for (int i=0; i<nops; ++i) {
			if (opset[i] == 0) {
				assert(ht->num_records() < ht->table_size());
				assert(!testset->empty());
				k = testset->back();
				r = ht->insert(k, k);
				if (r == hashtable::result::SUCCESS ||
				    r == hashtable::result::REBUILD) {
					testset->pop_back();
					inserted->push_back(k);
				} else
					std::cerr << "Duplicate insert!\n";
			} else {
				assert(!inserted->empty());
				assert(!delorder->empty());
				k = (*inserted)[delorder->back()];
				/* uint32_t v; // check
				   assert(ht->query(k,&v)); 
				   assert(v == k); */
				r = ht->remove(k);
				if (r != hashtable::result::FAILURE) {
					(*inserted)[delorder->back()] =
						inserted->back();
					inserted->pop_back();
					delorder->pop_back();
				}
				else { std::cerr << "What?\n"; }
			}

			if (r == hashtable::result::REBUILD) {
				t2 = steady_clock::now();
				insert_time += t2 - t1;
				ht->rebuild();
				t1 = steady_clock::now();
				rebuild_time += t1 - t2;
			}
		}
		insert_time += steady_clock::now() - t1;

		insert_times->push_back(insert_time);
		rebuild_times->push_back(rebuild_time);
	}

	void float_timer(hashtable *ht,
                         std::vector<uint32_t> *testset,
	                 std::vector<uint32_t> *inserted,
	                 std::vector<duration<double>> *optimes,
	                 std::vector<duration<double>> *insert_times,
			 std::vector<duration<double>> *rebuild_times)
	{
		time_point<steady_clock> t1,t2;
		ht->rebuild(); 
		ht->reset_perf_counts();
		cout << "Timing floating operations with rebuilds.\n";

		for (int i=0; i<ntests; ++i) {
			cout << i+1 << "/" << ntests << std::flush;
			// operations 
			std::vector<uint8_t> opset;
			opset.reserve(nops);
			for (int j = (i&1); j < nops; ++j) opset.push_back(j & 1);
			// std::shuffle(std::begin(opset), std::end(opset), rng);

			// deletion order
			std::vector<uint32_t> delorder;
			delorder.reserve(nops/2+1);
			std::uniform_int_distribution<> U(0,inserted->size()/2);
			while((int)delorder.size() < nops/2+1) 
				delorder.push_back(U(rng));

			ht->rebuilds = 0;
			// timed section - floating ops and rebuild
			t1 = steady_clock::now();
			floating(ht, testset, inserted, &delorder, opset,
			         insert_times, rebuild_times);
			t2 = steady_clock::now();
			optimes->push_back(t2 - t1);

			cout << " Time: " << optimes->back()
			     << ", Inserting: " << insert_times->back()
			     << ", Rebuilding: " << rebuild_times->back()
			     << std::endl;

			ht->rebuild();
			ht->reset_perf_counts();
		}
		cout << std::endl;
	}

	std::ostream& dump_amort_stats(std::ostream &o = std::cout) const
	{
		for (amort_stats_t q : stats) {
			o << q.nops << ", "
			  << q.x << ", "
			  << q.n << ", "
			  << q.mean_ops_time << ", "
			  << q.median_ops_time << ", "
			  << q.total_ops_time << ", "
			  << q.mean_ins_time << ", "
			  << q.total_ins_time << ", "
			  << q.mean_rb_time << ", "
			  << q.total_rb_time << ", "
			  << q.rw << ", "
			  << q.rb << ", "
			  << q.alpha << '\n';
		}

		return o;
	}

	void run_test()
	{
		std::vector <uint32_t> testset;
		std::vector <uint32_t> inserted;
		hashtable ht(next_prime(b));

		gen_testset(&testset, ht.table_size());

		ht.set_max_load_factor(1.0);

		vector <duration<double>> op_times, ins_times, rb_times;
		double lf = 1.0 - (1.0 / x);

		cout << ht.table_type() << " "
		     << ", slots=" << ht.table_size()
		     << ", x=" << x
		     << ", rec width=" << ht.rec_width()
		     << " [k:" << ht.key_width()
		     << ",v:" << ht.value_width()
		     << "], state width=" << ht.state_width()
		     << std::endl;

		loadtable(&ht, &testset, &inserted, lf);
		float_timer(&ht, &testset, &inserted,
		            &op_times, &ins_times, &rb_times);

		amort_stats_t q {
			.b                = b,
			.x                = x,
			.nops             = nops,
			.ops_time         = op_times,
			.mean_ops_time    = mean(op_times),
			.median_ops_time  = median(op_times),
			.total_ops_time   = sum(op_times),
			.mean_ins_time    = mean(ins_times),
			.total_ins_time   = sum(ins_times),
			.mean_rb_time     = mean(rb_times),
			.total_rb_time    = sum(rb_times),
			.rw               =
			       (unsigned int)(ht.table_size() / (4*x)),
			.rb               =
			       (unsigned int)(nops * 4 * x / ht.table_size()),
			.alpha            = ht.load_factor(),
			.n                = ht.table_size(),
		};

		stats.push_back(q);
	}

	public:
	amorttester(pcg64 &r, double x, uint64_t b, int no, int nt)
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
