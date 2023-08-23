#ifndef REBUILDTESTER_HPP
#define REBUILDTESTER_HPP

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

#define KEY_MAX 2000000000L

//#define QUIET

using std::chrono::duration;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::uniform_int_distribution;
using std::cout, std::vector;

template <typename hashtable>
class rebuildtester {
	private:
	std::string type;
	pcg64 &rng;
	const std::vector<int> &xs;
	const std::vector<uint64_t> &bs;
	int ntests;

	struct rebuild_stats_t {
		std::vector<duration<double>> ops_time;
		std::vector<int> rebuild_windows;
		std::vector<duration<double>> rebuild_time;
		double mean_ops_time;
		double median_ops_time;
		double mean_rebuild_time;
		double median_rebuild_time;
		double alpha;
		int x;
		std::size_t n;
	};
	std::vector<rebuild_stats_t> stats;

	inline void
	loadrebuild(hashtable *ht)
	{
		// specialized function at the bottom of this file
		// because rebuilding really speeds up loading for graveyards
	}

	// generate random numbers and insert into the table.
	// maintain list of all keys inserted until target load factor reached
	void
	loadtable(hashtable *ht, std::vector<int> *keys, double lf)
	{
		double start = ht->load_factor();
		int interval = (lf - ht->load_factor()) * ht->table_size() / 20;
		int stat_timer = interval;
		uint64_t k;

		uniform_int_distribution<uint64_t> data(0,KEY_MAX);
#ifndef QUIET
		cout << "Load " << ht->load_factor() << " -> " << lf << "\n";
#endif 
		while (ht->load_factor() < lf) {
			k = data(rng);
			using result = hashtable::result;
			result r = ht->insert(k, k/2);
			if (r == result::SUCCESS || r == result::REBUILD)
				keys->push_back(k);
			if (r == result::REBUILD)
				loadrebuild(ht);

#ifndef QUIET
			if (--stat_timer == 0) {
				stat_timer = interval;
				cout << "\r["
				     << 100.0*(ht->load_factor() - start)
				        / (lf - start)
				     << "%]   " << std::flush;
			}
#endif
		}
#ifndef QUIET
		cout << "\r[done]     \n";
#endif
	}

	void
	floating(hashtable *ht, std::vector<int> *keys)
	{
		int op, i, k;
		int ins = 0, del = 0;
		uniform_int_distribution<uint64_t> data(0,KEY_MAX);
		uniform_int_distribution<int> operation(0,99);
		
		// randomly insert or delete until the rebuild window is hit
		using res = hashtable::result;
		res r;
		do {
			op = operation(rng);
			
			if (op < 50) {
				assert(ht->num_records() < ht->table_size());
				do {
					k = data(rng);
					r = ht->insert(k, 42);
				} while (r == res::DUPLICATE);
				keys->push_back(k);
				++ins;
			} else {
				assert(!keys->empty());
				uniform_int_distribution<size_t>
					p(0,keys->size()-1);
				i = p(rng);
				k = (*keys)[i];

				r = ht->remove(k);
				if (r != res::FAILURE) {
					std::swap((*keys)[i], keys->back());
					keys->pop_back();
				}
				++del;
			}
		} while (r != res::REBUILD);
	}

	void float_rebuild_timer(hashtable *ht, std::vector<int> *keys,
				 std::vector<duration<double> > *optimes,
				 std::vector<duration<double> > *rbtimes,
				 std::vector<int> *rbwindows)
	{
		time_point<steady_clock> start, end;
		ht->rebuild();  // start from a "good" state
		ht->reset_perf_counts();
#ifndef QUIET
		cout << "timing operations and rebuilds: ";
#endif
		for (int i=0; i<ntests; ++i) {
#ifndef QUIET
			cout << i+1 << std::flush;
#endif
			rbwindows->push_back(ht->get_rebuild_window());

			start = steady_clock::now();
			floating(ht, keys);
			end = steady_clock::now();
			optimes->push_back(end - start);

#ifndef QUIET
			cout << "...rebuild... " << std::flush;
#endif
			start = steady_clock::now();
			ht->rebuild();
			end = steady_clock::now();
			rbtimes->push_back(end - start);

			ht->reset_perf_counts();
		}
#ifndef QUIET
		cout << std::endl;
#endif
	}

	std::ostream& dump_rebuild_stats(std::ostream &o = std::cout) const
	{
		o << "\n----- " << type
		     << " --------------------------------\n";
		o << "Rebuild windows, Floating ops times, Mean, Median, "
		     << "Rebuild times, Mean, Median, Loadfactor, x, n\n";

		for (rebuild_stats_t q : stats) {
			o << q.rebuild_windows << ", "
			  << q.ops_time << ", "
			  << q.mean_ops_time << ", "
			  << q.median_ops_time << ", "
			  << q.rebuild_time << ", "
			  << q.mean_rebuild_time << ", "
			  << q.median_rebuild_time << ", "
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
			vector<int> keys;
			type = ht.table_type();
			cout << type << "\n";

			uint64_t size = ht.table_size();
			ht.set_max_load_factor(1.0);
			keys.reserve(size);
			for (auto x : xs) {
				double lf = 1.0 - (1.0 / x);
				cout << ht.table_type() << " size: " << size
				     << ", x=" << x << " (lf=" << lf << ") "
				     << std::endl;

				loadtable(&ht, &keys, lf);

				vector <duration<double> > op_times;
				vector <duration<double> > rb_times;
				vector <int> rbwindows;
				float_rebuild_timer(&ht, &keys, &op_times,
				                    &rb_times, &rbwindows);

				rebuild_stats_t q {
					.ops_time            = op_times,
					.rebuild_windows     = rbwindows,
					.rebuild_time        = rb_times,
					.mean_ops_time       = mean(op_times),
					.median_ops_time     = median(op_times),
					.mean_rebuild_time   = mean(rb_times),
					.median_rebuild_time = median(rb_times),
					.alpha               = ht.load_factor(),
					.x                   = x,
					.n                   = ht.table_size(),
				};

				stats.push_back(q);
			}
		}
	}

	public:
	rebuildtester(pcg64 &r, std::vector<int> const &x,
	              std::vector<uint64_t> const &b, int nt)
	             : rng(r), xs(x), bs(b), ntests(nt) {
		run_test();
	}

	friend std::ostream&
	operator<<(std::ostream& os, rebuildtester const& h) {
		return h.dump_rebuild_stats(os);
	}
};

template<> inline void
rebuildtester<graveyard_aos<>>::loadrebuild(graveyard_aos<> *ht)
{
	ht->rebuild();
}

template<> inline void
rebuildtester<graveyard_soa<>>::loadrebuild(graveyard_soa<> *ht)
{
	ht->rebuild();
}

#endif
