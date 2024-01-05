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

#define SOA
#define GRAVEYARD

int main(int argc, char **argv)
{
	const vector<int> fullxs{2,3,4,5,6,7,8,9,10,15,20,25,
			    30,40,50,60,70,80,90,100,200,300,400,500,700,1000};
	const vector<uint64_t> fullbs { 10'000, 100'000, 500'000,
		                      1'000'000, 5'000'000,
		                      10'000'000, 50'000'000,
		                      100'000'000, 500'000'000,
		                      1'000'000'000 };
	const vector<int> quickxs{2,3,4};
	const vector<uint64_t> quickbs{1'000'000};

	const auto &xs = fullxs;
	const auto &bs = quickbs;
	const int nt = 50;              // number of tests to average over

#ifdef SOA
#       ifdef GRAVEYARD
	for (auto b : bs) {
		std::ofstream f(std::to_string(b/1000) +
				"_graveyard_soa_rebuilds");
		f << rebuildtester<graveyard_soa<>>
			(rng, xs, vector<uint64_t>{b}, nt);
	}
#       endif
#       ifdef ORDERED
	for (auto b : bs) {
		std::ofstream f(std::to_string(b/1000) +
				"_ordered_soa_rebuilds");
		f << rebuildtester<ordered_soa<>>
			(rng, xs, vector<uint64_t>{b}, nt);
	}
#       endif
#       ifdef LINEAR
	for (auto b : bs) {
		std::ofstream f(std::to_string(b/1000) +
				"_linear_soa_rebuilds");
		f << rebuildtester<linear_soa<>>
			(rng, xs, vector<uint64_t>{b}, nt);
	}
#       endif
#endif

#ifdef AOS
#       ifdef GRAVEYARD
	for (auto b : bs) {
		std::ofstream f(std::to_string(b/1000) +
				"_graveyard_aos_rebuilds");
		f << rebuildtester<graveyard_aos<>>
			(rng, xs, vector<uint64_t>{b}, nt);
	}
#       endif
#       ifdef ORDERED
	for (auto b : bs) {
		std::ofstream f(std::to_string(b/1000) +
				"_ordered_aos_rebuilds");
		f << rebuildtester<ordered_aos<>>
			(rng, xs, vector<uint64_t>{b}, nt);
	}

#       endif
#       ifdef LINEAR
	for (auto b : bs) {
		std::ofstream f(std::to_string(b/1000) +
				"_linear_aos_rebuilds");
		f << rebuildtester<linear_aos<>>
			(rng, xs, vector<uint64_t>{b}, nt);
	}
#       endif
#endif
	cout << "Complete\n";

	return 0;
}

