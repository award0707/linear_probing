#include <iostream>

#include "pcg_random.hpp"
#include "primes.h"
#include "util.h"

#include "testers/querytester.hpp"
#include "graveyard.h"
#include "ordered.h"
#include "linear.h"

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

int main(int argc, char **argv)
{
	const vector<uint64_t> bs { 10'000'000 };
	vector<int> xs;

	for(int i=2; i<=200; i++)
		xs.push_back(i);

	const int nq = 1'000'000;       // queries per test
	const int nt = 10;              // number of tests to average over

	std::ofstream f("x_querytester_out");
	f << querytester<graveyard_soa<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<graveyard_aos<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<ordered_soa<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<ordered_aos<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<linear_soa<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<linear_aos<int, int> >(rng, xs, bs, nq, nt);
	f.close();

	std::ofstream f("x_floattester_out");
	f << querytester<graveyard_soa<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<graveyard_aos<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<ordered_soa<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<ordered_aos<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<linear_soa<int, int> >(rng, xs, bs, nq, nt);
	f << querytester<linear_aos<int, int> >(rng, xs, bs, nq, nt);
	f.close();

	cout << "Complete\n";

	return 0;
}

