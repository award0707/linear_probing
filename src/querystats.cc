#include <iostream>

#include "pcg_random.hpp"
#include "primes.h"
#include "util.h"

#include "testers/querytester.hpp"
#include "testers/one_rb_querytester.hpp"
#include "graveyard.h"
#include "ordered.h"
#include "linear.h"

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

int main(int argc, char **argv)
{
	const vector<int> fullxs{2,3,4,5,6,7,8,9,10,15,20,25,
			    30,40,50,60,70,80,90,100,200,400,700,1000};
	const vector<uint64_t> fullbs { 10'000, 100'000, 500'000,
		                      1'000'000, 5'000'000,
		                      10'000'000, 50'000'000,
		                      100'000'000, 500'000'000,
		                      1'000'000'000 };
	const vector<int> testxs{400};
	const vector<uint64_t> testbs{100'000'000};

	const auto &xs = fullxs;
	const auto &bs = fullbs;
	const int nq = 1'000'000;       // queries per test
	const int nt = 10;              // number of tests to average over

	{ std::ofstream f("querybench_graveyard_aos");
	  f << querytester<graveyard_aos<>>(rng, xs, bs, nq, nt, 0); }

//	{ std::ofstream f("querybench_stoprebuilding_aos");
//	  f << one_rb_querytester<graveyard_aos<>>(rng,xs,bs,nq,nt,0); }
/*
	{ std::ofstream f("querybench_graveyard_aos");
	  f << querytester<graveyard_aos<>>(rng, xs, bs, nq, nt, 0); }

	{ std::ofstream f("querybench_ordered_soa");
	  f << querytester<ordered_soa<>>(rng, xs, bs, nq, nt, 0); }

	{ std::ofstream f("querybench_ordered_aos");
	  f << querytester<ordered_aos<>>(rng, xs, bs, nq, nt, 0); }

	{ std::ofstream f("querybench_linear_soa");
	  f << querytester<linear_soa<>>(rng, xs, bs, nq, nt, 0); }

	{ std::ofstream f("querybench_linear_aos");
	  f << querytester<linear_aos<>>(rng, xs, bs, nq, nt, 0); }
*/
	return 0;
}

