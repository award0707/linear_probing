#include <iostream>

#include "pcg_random.hpp"
#include "primes.h"
#include "util.h"

#include "testers/rebuildtester.hpp"
#include "graveyard.h"
#include "ordered.h"
#include "linear.h"

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

int main(int argc, char **argv)
{
	const vector<int> fullxs{2,3,4,5,6,7,8,9,10,15,20,25,
			    30,40,50,60,70,80,90,100,200,300,400,500,700,1000};
	const vector<uint64_t> fullbs { 10'000, 100'000, 500'000,
		                      1'000'000, 5'000'000,
		                      10'000'000, 50'000'000,
		                      100'000'000, 500'000'000,
		                      1'000'000'000 };
	const vector<int> testx{2,3,4};
	const vector<uint64_t> testb{100000, 200000};

	const auto &xs = fullxs;
	const auto &bs = fullbs;
	const int nt = 5;              // number of tests to average over

	std::ofstream f("output_rebuildstats");
	f << rebuildtester<graveyard_soa<int, int>>(rng, xs, bs, nt);
	f << rebuildtester<graveyard_aos<int, int>>(rng, xs, bs, nt);
	f << rebuildtester<ordered_soa<int, int>>(rng, xs, bs, nt);
	f << rebuildtester<ordered_aos<int, int>>(rng, xs, bs, nt);
	f << rebuildtester<linear_soa<int, int>>(rng, xs, bs, nt);
	f << rebuildtester<linear_aos<int, int>>(rng, xs, bs, nt);
	f.close();

	cout << "Complete\n";

	return 0;
}

