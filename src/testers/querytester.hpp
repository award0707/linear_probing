#ifndef QUERYTESTER_HPP
#define QUERYTESTER_HPP

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
#include "linear.h"

#define KEY_MAX 2000000000L

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

	void loadtable(hashtable *ht, std::vector<int> *keys, double lf)
	{
		double start = ht->load_factor();
		int interval = (lf - ht->load_factor()) * ht->table_size() / 20;
		int stat_timer = interval;
		uint64_t k;

		uniform_int_distribution<uint64_t> data(0,KEY_MAX);
		cout << "Loading: " << ht->load_factor() << " -> "
		     << lf << "\n";

		while(ht->load_factor() < lf) {
			k = data(rng);
			using result = hashtable::result;
			result r = ht->insert(k, k/2);
			if (r == result::SUCCESS || r == result::REBUILD) 
				keys->push_back(k);
			
			if (r == result::REBUILD) 
				ht->rebuild();

			if (--stat_timer == 0) {
				stat_timer = interval;
				cout << "\r["
				     << 100.0 * (ht->load_factor() - start)
				        / (lf - start)
				     << "%]   " << std::flush;
			}
		}

		cout << "\r[done]     \n";
	}

	void querying(hashtable *ht, const std::vector<int> &keys,
	              int nq, int f_pct)
	{
		int k, v;
		uint64_t fails = 0;

		uniform_int_distribution<size_t> p(0,keys.size()-1);
		uniform_int_distribution<uint64_t> data(0,KEY_MAX);
		uniform_int_distribution<int> fail(0,99);
		
		for (int i = 0; i < nq; ++i) {
			if (fail(rng) >= f_pct)
				k = keys[p(rng)];
			else
				k = data(rng);

			if (!ht->query(k, &v)) ++fails;
		}

		if (f_pct == 0)
			assert (fails == 0);
	}

	void uniquething() {
	}

	void querytimer(hashtable *ht, const std::vector<int> &keys,
			std::vector<duration<double> > *d,
			int nq, int f_pct)
	{
		time_point<steady_clock> start, end;
		ht->reset_perf_counts();

		for (int i=0; i<ntests; ++i) {
			cout << i+1 << std::flush;

			start = steady_clock::now();
			querying(ht, keys, nq, f_pct);
			end = steady_clock::now();

			cout << "... ";

			d->push_back(end - start);
			ht->reset_perf_counts();
		}
		cout << std::endl;
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
			vector<int> keys;

			uniquething();
			uint64_t size = ht.table_size();
			ht.set_max_load_factor(1.0);
			keys.reserve(size);
			for (auto x : xs) {
				double lf = 1.0 - (1.0 / x);
				cout << "Table size: " << size
				     << ", x=" << x
				     << " (lf=" << lf << "): ";

				loadtable(&ht, &keys, lf);

				vector <duration<double> > times;
				querytimer(&ht, keys, &times, nqueries, 0);

				query_stats_t q {
					.nqueries            = nqueries,
					.failrate            = 0,
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

		// failed query test section
		{
			uint64_t b = 10'000'000;
			int x = 500;
			double lf = 1.0 - (1.0 / x);
			hashtable ht(next_prime(b));
			vector<int> keys;

			uint64_t size = ht.table_size();
			ht.set_max_load_factor(1.0);
			keys.reserve(size);
			loadtable(&ht, &keys, lf);
			for (int fr = 0; fr < 100; fr += 10) {
				cout << "Table size: " << size
				     << ", x=" << x
				     << " (lf=" << lf << "), fail rate "
				     << fr << ": ";

				vector <duration<double> > times;
				querytimer(&ht, keys, &times, nqueries, fr);

				query_stats_t q {
					.nqueries            = nqueries,
					.failrate            = fr,
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
	            std::vector<uint64_t> const &b, int nq, int nt)
	           : rng(r), xs(x), bs(b), nqueries(nq), ntests(nt) {
		run_test();
	}

	friend
	std::ostream& operator<<(std::ostream& os, querytester const& h) {
		return h.dump_query_stats(os);
	}
};

#endif

