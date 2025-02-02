#include <iostream>

#include "pcg_random.hpp"
#include "primes.h"
#include "util.h"

#include "testers/amorttester.hpp"
#include "graveyard.h"
#include "ordered.h"
#include "linear.h"

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

#define AOS

int main(int argc, char **argv)
{
	const vector<int> xs { 10 };
	const vector<uint64_t> bs {
		 10'000,
	};

	const int nops = 1'000'000;
	const int nt = 50;              // number of tests to average over

#ifdef SOA
	for (auto x : xs) {
		std::ofstream f(std::to_string(x) + "_soa_amort");
		f << amorttester<graveyard_soa<>>
			(rng, vector<int>{x}, bs, nops, nt);
	}
#endif

#ifdef AOS
	for (auto x : xs) {
		std::ofstream f(std::to_string(x) + "_aos_amort");
		f << amorttester<graveyard_aos<>>
			(rng, vector<int>{x}, bs, nops, nt);
	}
#endif
	cout << "Complete\n";

	return 0;
}

