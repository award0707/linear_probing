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
#include "ordered.h"
#include "linear.h"

#define KEY_MAX 2000000000L
#define NTESTS 1

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

using hashtable = graveyard_aos<uint32_t,int>;

using std::chrono::duration;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::uniform_int_distribution;
using std::cout, std::vector;

struct float_stats_t {
	int nops;
	std::vector<duration<double> > ops_time;
	double mean_ops_time;
	double median_ops_time;
	double alpha;
	int x;
	std::size_t n;
};

// generate random numbers and insert into the table.
// maintain list of all keys inserted until target load factor reached
void loadtable(hashtable *ht, std::vector<int> *keys, double lf)
{
	double start = ht->load_factor();
	int interval = (lf - ht->load_factor()) * ht->table_size() / 20;
	int stat_timer = interval;
	uint64_t k;

	uniform_int_distribution<uint64_t> data(0,KEY_MAX);
	cout << "Loading: " << ht->load_factor() << " -> " << lf << "\n";

	while (ht->load_factor() < lf) {
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
			     << 100.0*(ht->load_factor() - start)/(lf - start)
			     << "%]   " << std::flush;
		}
	}

	cout << "\r[done]     \n";
}

void floating(hashtable *ht, std::vector<int> *keys, int nops)
{
	int i, k;
	int ins = 0, del = 0;
	uniform_int_distribution<uint64_t> data(0,KEY_MAX);
	uniform_int_distribution<int> operation(0,99);
	
	// randomly insert or delete until the rebuild window is hit
	using res = hashtable::result;
	res r;
	do {
		if (operation(rng) < 50) {
			assert(ht->num_records() < ht->table_size());
			do {
				k = data(rng);
				r = ht->insert(k, 42);
			} while (r == res::DUPLICATE);
			keys->push_back(k);
			++ins;
		} else {
			assert(!keys->empty());
			uniform_int_distribution<size_t> p(0,keys->size()-1);
			i = p(rng);
			k = (*keys)[i];

			r = ht->remove(k);
			if (r != res::FAILURE) {
				std::swap((*keys)[i], keys->back());
				keys->pop_back();
			}
			++del;
		}

		if (r == hashtable::result::REBUILD) ht->rebuild();
		if (!(nops % 10'000)) std::cerr << "\rnops = " << nops << std::flush;
	} while (--nops);
	cout << "+"<<ins<<",-"<<del<<" : " << ht->table_size() - ht->num_records() << " free\n";
}

void float_timer(hashtable *ht, std::vector<int> *keys,
                         std::vector<duration<double> > *optimes, int nops)
{
	time_point<steady_clock> start, end;

	cout << "Floating op timing: ";
	for (int i=0; i<NTESTS; ++i) {
		cout << "...set up... ";
		ht->rebuild();
		ht->reset_perf_counts();

		cout << i+1 << ", rbw=" << ht->get_rebuild_window() << " ";

		start = steady_clock::now();
		floating(ht, keys, nops);
		end = steady_clock::now();
		optimes->push_back(end - start);
	}
	cout << std::endl;
}

double mean(std::vector<duration<double> > v)
{
	return (std::reduce(v.begin(), v.end()) / v.size()).count();
}

double median(std::vector<duration<double> > dv)
{
	double med;
	vector<double> v;

	for (auto i : dv) v.push_back(i.count());
	int n = v.size() / 2.0;

	std::nth_element(v.begin(), v.begin() + n, v.end());
	med = v[n];

	if ((v.size() % 2) == 0) {
		auto max = std::max_element(v.begin(), v.begin() + n);
		med = (*max + med) / 2.0;
	}

	return med;
}

std::ostream& operator<<(std::ostream& os,
                         const std::vector<duration<double> >& v)
{
	os << "[";
	for (auto &i : v) os << " " << i.count();
	os << "]";
        return os;
}

void dump_float_stats(const vector<float_stats_t> &v,
                              std::ostream &o = std::cout)
{
	for (float_stats_t q : v) {
		o << q.nops << ", "
		  << q.ops_time << ", "
		  << q.mean_ops_time << ", "
		  << q.median_ops_time << ", "
		  << q.alpha << ", "
		  << q.x << ", "
		  << q.n << '\n';
	}
}

int main(int argc, char **argv)
{
	vector<float_stats_t> stats;

	const vector<int> xs{5,6,7,8,9,10,15,20,25,
	               30,40,50,60,70,80,90,100,200,400,700,1000};
	const vector<uint64_t> bs { //10'000'000, 50'000'000, 100'000'000,
	                            //250'000'000, 500'000'000 };
	                            750'000'000, 1'000'000'000 };
	
	for (auto b : bs) {
		hashtable ht(next_prime(b));
		vector<int> keys;
	        int nops = 10'000'000;

		uint64_t size = ht.table_size();
		ht.set_max_load_factor(1.0);
		keys.reserve(size);
		for (auto x : xs) {
			double lf = 1.0 - (1.0 / x);
			cout << "Table size: " << size
			     << ", x=" << x
			     << " (lf=" << lf << "): ";

			loadtable(&ht, &keys, lf);

			vector <duration<double> > op_times;
			float_timer(&ht, &keys, &op_times, nops);

			float_stats_t q {
			        .nops                = nops,
			        .ops_time            = op_times,
			        .mean_ops_time       = mean(op_times),
			        .median_ops_time     = median(op_times),
			        .alpha               = ht.load_factor(),
			        .x                   = x,
			        .n                   = ht.table_size(),
			};

			stats.push_back(q);
		}
	}

	cout << "\n------------------------------------------------------\n";
	cout << "# ops, floating ops times, Mean, Median, Loadfactor, x, n\n";
	dump_float_stats(stats);

	return 0;
}

