#include <iostream>

#include "pcg_random.hpp"
#include "primes.h"
#include "util.h"

#include "testers/querytester.hpp"
#include "testers/floattester.hpp"
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
/*
	std::ofstream f("x_querytester_out");
	f << querytester<graveyard_soa<> >(rng, xs, bs, nq, nt,0);
	f << querytester<graveyard_aos<> >(rng, xs, bs, nq, nt,0);
	f << querytester<ordered_soa<> >(rng, xs, bs, nq, nt,0);
	f << querytester<ordered_aos<> >(rng, xs, bs, nq, nt,0);
	f << querytester<linear_soa<> >(rng, xs, bs, nq, nt,0);
	f << querytester<linear_aos<> >(rng, xs, bs, nq, nt,0);
	f.close();
*/
	std::ofstream f("x_floattester_out");
	f << floattester<graveyard_soa<>>(rng, xs, bs, nq, nt);
	f << floattester<graveyard_aos<>>(rng, xs, bs, nq, nt);
	f << floattester<ordered_soa<>>(rng, xs, bs, nq, nt);
	f << floattester<ordered_aos<>>(rng, xs, bs, nq, nt);
	f << floattester<linear_soa<>>(rng, xs, bs, nq, nt);
	f << floattester<linear_aos<>>(rng, xs, bs, nq, nt);
	f.close();

	cout << "Complete\n";

	return 0;
}

