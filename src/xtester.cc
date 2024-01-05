#include <iostream>

#include "pcg_random.hpp"
#include "primes.h"
#include "util.h"

#include "testers/querytester.hpp"
#include "testers/floattester.hpp"
#include "testers/one_rb_querytester.hpp"
#include "graveyard.h"
#include "ordered.h"
#include "linear.h"

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(42u);

// range of load factors to test
#define START 2
#define END 400

int main(int argc, char **argv)
{
	const vector<uint64_t> bs
		{ 1'000'000,
		};
	vector<int> xs;
	vector<int> mxs { 5, 10, 20, 50, 100, 150, 250, 400 };

	for(int i=START; i<=END; i++)
		xs.push_back(i);

	const int nq = 1'000'000;       // queries per test
	const int nt = 10;              // number of tests to average over

	for (auto b: bs) {
		std::ofstream f(std::to_string(b/1000) + "_aos_query_xtester");
		f << querytester<graveyard_aos<>>
			(rng, xs, std::vector<uint64_t>{b}, nq, nt, 0);
	}

/*	for (auto b: bs) {
		std::ofstream f(std::to_string(b/1000) + "_soa_query_xtester");
		f << querytester<graveyard_soa<>>
			(rng, xs, std::vector<uint64_t>{b}, nq, nt, 0);
	}*/
/*/
	for (auto b: bs) {
		for (auto mx: mxs) {
		std::ofstream f(std::to_string(b/1000) + "_rb_" +
				std::to_string(mx) + "_aos_query_xtester_1r");
		f << one_rb_querytester<graveyard_aos<>>
			(rng, xs, std::vector<uint64_t>{b}, nq, nt, 0, mx);
		}
	}
	*/
//	f << querytester<ordered_soa<> >(rng, xs, bs, nq, nt,0);
//	f << querytester<ordered_aos<> >(rng, xs, bs, nq, nt,0);
//	f << querytester<linear_soa<> >(rng, xs, bs, nq, nt,0);
//	f << querytester<linear_aos<> >(rng, xs, bs, nq, nt,0);

	/*
	std::ofstream f("x_floattester_out");
	f << floattester<graveyard_soa<>>(rng, xs, bs, nq, nt);
	f << floattester<graveyard_aos<>>(rng, xs, bs, nq, nt);
	f << floattester<ordered_soa<>>(rng, xs, bs, nq, nt);
	f << floattester<ordered_aos<>>(rng, xs, bs, nq, nt);
	f << floattester<linear_soa<>>(rng, xs, bs, nq, nt);
	f << floattester<linear_aos<>>(rng, xs, bs, nq, nt);
	f.close();
	*/
	cout << "Complete\n";

	return 0;
}

