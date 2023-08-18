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

#define KEY_MAX 2000000000L
#define NTESTS 10

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

using graveyard = graveyard_aos<int,int>;
using hashtable = ordered_aos;

using std::chrono::duration;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::uniform_int_distribution;
using std::cout, std::vector;

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

	while(ht->load_factor() < lf) {
		k = data(rng);
		using result = hashtable::result;
		result r = ht->insert(k, k/2);
		if (r == result::SUCCESS || r == result::REBUILD) {
			keys->push_back(k);
		}

		if (--stat_timer == 0) {
			stat_timer = interval;
			cout << "\r["
			     << 100.0*(ht->load_factor() - start)/(lf - start)
			     << "%]   " << std::flush;
		}
	}

	cout << "\r[done]     \n";
}

void querying(ordered_aos *ht, const std::vector<int> &keys, int nq, int f_pct)
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

	if ((double)fails/nq*100.0 > f_pct)
		cout << "[True %" << (double)fails/nq*100.0 << "] ";
}

void querying(graveyard *ht, const std::vector<int> &keys, int nq, int f_pct)
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

	if ((double)fails/nq*100.0 > f_pct)
		cout << "[True %" << (double)fails/nq*100.0 << "] ";
}

void querytimer(ordered_aos *ht, const std::vector<int> &keys,
                std::vector<duration<double> > *d,
                int nq, int f_pct)
{
	time_point<steady_clock> start, end;
	ht->reset_perf_counts();

	cout << "Query timing some other hash table: ";
	for (int i=0; i<NTESTS; ++i) {
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

void querytimer(graveyard *ht, const std::vector<int> &keys,
                std::vector<duration<double> > *d,
                int nq, int f_pct)
{
	time_point<steady_clock> start, end;
	ht->reset_perf_counts();

	cout << "Query timing a graveyard_aos hash table: ";
	for (int i=0; i<NTESTS; ++i) {
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

void dump_query_stats(const vector<query_stats_t> &v,
                      std::ostream &o = std::cout)
{
	for (query_stats_t q : v) {
		o << q.nqueries << ", "
		  << q.failrate << ", "
		  << q.query_time << ", "
		  << q.mean_query_time << ", "
		  << q.median_query_time << ", "
		  << q.alpha << ", "
		  << q.x << ", "
		  << q.n << '\n';
	}
}

int main(int argc, char **argv)
{
	vector<query_stats_t> querystats;

	const vector<int> xs{2,3,4,5,6,7,8,9,10,15,20,25,
	               30,40,50,60,70,80,90,100,200,400,700,1000};
	const vector<uint64_t> bs { 10'000, 100'000, 500'000,
	                      1'000'000, 5'000'000,
	                      10'000'000, 50'000'000,
	                      100'000'000, 500'000'000,
	                      1'000'000'000 };
	const int nqueries = 1'000'000;
	
	for (auto b : bs) {
		hashtable ht(b);
		vector<int> keys;

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

	{
		uint64_t b = 10'000'000;
		int x = 500;
		double lf = 1.0 - (1.0 / x);
		hashtable ht(b);
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

	cout << "\n------------------------------------------------------\n";
	cout << "Queries/trial, Fail%, Trial times, Mean, Median, Loadfactor, x, n\n";
	dump_query_stats(querystats);

	return 0;
}

