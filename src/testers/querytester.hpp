#ifndef QUERYTESTER_HPP
#define QUERYTESTER_HPP

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <unistd.h>
#include <vector>
#include <random>
#include <chrono>

#include "pcg_random.hpp"
#include "primes.h"
#include "linear.h"

using std::chrono::duration;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::uniform_int_distribution;
using std::cout, std::vector;

template <typename hashtable>
class querytester {
	private:

	std:: string type;
	pcg64 &rng;
	const std::vector<int> &xs;
	const std::vector<uint64_t> &bs;
	int nqueries;
	int ntests;
	int fail_pct;

	struct query_stats_t {
		int nqueries;
		int failrate;
		std::vector<duration<double> > query_time;
		double mean_query_time;
		double median_query_time;
		double alpha;
		int x;
		std::size_t n;
	};

	std::vector<query_stats_t> querystats;

	void loadtable(hashtable *ht, std::vector<uint32_t> *keys, double lf)
	{
		double start = ht->load_factor();
		int interval = (lf - ht->load_factor()) * ht->table_size() / 20;
		int stat_timer = interval;
		uint32_t k;

		uniform_int_distribution<uint32_t> data(0,UINT32_MAX);
		cout << "Loading: " << ht->load_factor() << " -> "
		     << lf << " [      %]";

		std::ios old(nullptr);
		old.copyfmt(cout);
		while(ht->load_factor() < lf) {
			k = data(rng);
			using result = hashtable::result;
			result r = ht->insert(k, k>>1);
			switch(r) {
			case result::SUCCESS:
				keys->push_back(k);
				break;
			case result::REBUILD:
				keys->push_back(k);
				cout << " REBUILD" << std::flush;
				ht->rebuild();
				cout << "\b\b\b\b\b\b\b\b" << std::flush;
				break;
			case result::FULLTABLE: // this should never happen
				std::cerr << "Table full!\n";
				return;
			default:
				break;
			}

			if (--stat_timer == 0) {
				stat_timer = interval;
				cout << "\b\b\b\b\b\b\b\b"
				     << std::setw(6) << std::fixed
				     << std::setprecision(2)
				     << 100 * (ht->load_factor() - start)
				        / (lf - start)
				     << "%]" << std::flush;
			}
		}
		cout.copyfmt(old);
		cout << "\b\b\b\b\b\b\b\bdone]         \n";
	}

	void querying(hashtable *ht, const std::vector<uint32_t> &keys,
	              int nq, int f_pct)
	{
		uint32_t v;
		uint64_t fails = 0;
		int j = keys.size()-1;

		while (nq--) {
			if (!ht->query(keys[j], &v)) {
				std::cerr << "key #" << j << ": "
				          << keys[j] << " not found\n";
				++fails;
			}
			if (--j < 0) j=keys.size()-1;
		}
		
		if (f_pct == 0 && fails != 0) {
			std::cerr << fails << " erroneous fails! ";
			if (!ht->check_ordering())
				std::cerr << "Ordering was violated\n";
		}
	}

	void querytimer(hashtable *ht, vector<uint32_t> *keys,
			vector<duration<double>> *d, int nq, int f_pct)
	{
		time_point<steady_clock> start, end;
		uniform_int_distribution<uint32_t> data(0,UINT32_MAX);

		std::shuffle(std::begin(*keys), std::end(*keys), rng);
		// miss test: replace some of the stored keys with random keys
		if (f_pct > 0) 
			for (size_t i=0; i<keys->size()*f_pct/100.0; ++i) {
				keys->pop_back();
				keys->push_back(data(rng));
			}
		ht->reset_perf_counts();
		ht->rebuild();

		for (int i=0; i<ntests; ++i) {
			std::shuffle(std::begin(*keys), std::end(*keys), rng);
			//cout << i+1 << std::flush;

			// timed section: 'nq' queries
			start = steady_clock::now();
			querying(ht, *keys, nq, f_pct);
			end = steady_clock::now();
			// end timed section

			//cout << ".." << std::flush;

			d->push_back(end - start);
			ht->reset_perf_counts();
		}
		cout << std::flush;
	}

	std::ostream& dump_query_stats(std::ostream &o = std::cout) const
	{
		o << "\n----- " << type
		  << " -------------------------------\n";
		o << "Queries/trial, Fail%, Trial times, "
			"Mean, Median, Loadfactor, x, n\n";
		for (query_stats_t q : querystats) {
			o << q.nqueries << ", "
			  << q.failrate << ", "
			  << q.query_time << ", "
			  << q.mean_query_time << ", "
			  << q.median_query_time << ", "
			  << q.alpha << ", "
			  << q.x << ", "
			  << q.n << '\n';
		}
		return o;
	}

	void run_test()
	{
		for (auto b : bs) {
			hashtable ht(next_prime(b));
			type = ht.table_type();
			ht.set_max_load_factor(1.0);
			vector<uint32_t> keys;
			uint32_t size = ht.table_size();
			keys.reserve(size);

			for (auto x : xs) {
				double lf = 1.0 - (1.0 / x);
				cout << "Table size: " << size << ", x=" << x
				     << " (lf=" << lf << "): ";

				loadtable(&ht, &keys, lf);

				vector <duration<double>> times;
				querytimer(&ht, &keys, &times,
				           nqueries, fail_pct);

				query_stats_t q {
					.nqueries            = nqueries,
					.failrate            = fail_pct,
					.query_time          = times,
					.mean_query_time     = mean(times),
					.median_query_time   = median(times),
					.alpha               = ht.load_factor(),
					.x                   = x,
					.n                   = ht.table_size(),
				};

				querystats.push_back(q);
			}
		}
	}

	public:
	querytester(pcg64 &r, std::vector<int> const &x,
	            std::vector<uint64_t> const &b, int nq, int nt, int fp)
	           : rng(r), xs(x), bs(b), nqueries(nq), ntests(nt),
	             fail_pct(fp) {
		run_test();
	}

	friend
	std::ostream& operator<<(std::ostream& os, querytester const& h) {
		return h.dump_query_stats(os);
	}
};

#endif

