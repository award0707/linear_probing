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
#define NTESTS 10

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

using hashtable = graveyard_aos;

using std::chrono::steady_clock;
using std::chrono::time_point;
using std::uniform_int_distribution;
using std::cout, std::vector;

struct queue_stats_t {
	int rebuild_window;
	std::vector<int> max_rebuild_queue;
	double mean_mrq;
	double median_mrq;
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

void floating(hashtable *ht, std::vector<int> *keys)
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
	} while (r != res::REBUILD);
}

void queuetest(hashtable *ht, std::vector<int> *keys, std::vector<int> *mrq)
{
	ht->rebuild();   // start at the top of the next rebuild window
	ht->reset_perf_counts();

	cout << "Float and rebuild: ";
	for (int i=0; i<NTESTS; ++i) {
		cout << i+1 << std::flush;
		floating(ht, keys);
		assert(ht->failed_inserts == ht->duplicates);
		assert(ht->failed_removes == 0);

		cout << "... " << std::flush;
		ht->rebuild();

		mrq->push_back(ht->max_rebuild_queue);
		ht->reset_perf_counts();
	}
	cout << std::endl;
}

double mean(std::vector<int> v)
{
	return std::reduce(v.begin(), v.end()) / v.size();
}

double median(std::vector<int> v)
{
	double med;
	int n = v.size() / 2.0;

	std::nth_element(v.begin(), v.begin() + n, v.end());
	med = v[n];

	if ((v.size() % 2) == 0) {
		auto max = std::max_element(v.begin(), v.begin() + n);
		med = (*max + med) / 2.0;
	}

	return med;
}

void dump_queue_stats(const vector<queue_stats_t> &v,
                      std::ostream &o = std::cout)
{
	for (queue_stats_t q : v) {
		o << q.n << ", " << q.x << ", ";
		o << q.alpha << ", " << q.rebuild_window << ", ";

		o << "[";
		for (int x: q.max_rebuild_queue) o << ' ' << x;
		o << "], ";

		o << q.mean_mrq << ", " << q.median_mrq << "\n";
	}
}

int main(int argc, char **argv)
{
	vector<queue_stats_t> queuestats;
	vector<int> xs{2,3,4,5,6,7,8,9,10,15,20,25,
	               30,40,50,60,70,80,90,100,200,400,700,1000};
	vector<uint64_t> bs { 10'000, 100'000, 500'000,
	                      1'000'000, 5'000'000,
	                      10'000'000, 50'000'000,
	                      100'000'000, 500'000'000,
	                      1'000'000'000 };
	
	for (auto b : bs) {
		hashtable ht(next_prime(b));
		vector<int> keys;

		uint64_t size = ht.table_size();
		keys.reserve(size);
		for (auto x : xs) {
			double lf = 1.0 - (1.0 / x);
			ht.set_max_load_factor(1.0);
			cout << "Table size: " << size
			     << ", x=" << x
			     << " (lf=" << lf << "): ";

			loadtable(&ht, &keys, lf);

			vector <int> mrq;
			queuetest(&ht, &keys, &mrq);

			queue_stats_t q {
			        .rebuild_window    = ht.get_rebuild_window(),
			        .max_rebuild_queue = mrq,
			        .mean_mrq          = mean(mrq),
			        .median_mrq        = median(mrq),
			        .alpha             = ht.load_factor(),
			        .x                 = x,
			        .n                 = ht.table_size(),
			};

			queuestats.push_back(q);
		}
	}

	cout << "\n------------------------------------------------------\n";
	cout << "n, x, a, R, Max queue length array, Mean, Median\n";
	dump_queue_stats(queuestats);

	return 0;
}

