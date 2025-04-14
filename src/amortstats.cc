#include <iostream>
#include <string>

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
//#define SOA

int main(int argc, char **argv)
{
	vector<double> xs;
	const vector<uint64_t> bs { // 10'000,
		 25'000, 50'000, 75'000, 100'000,
		 250'000, 500'000, 750'000, 1'000'000,
		 2'500'000, 5'000'000, 7'500'000, 10'000'000,
		 25'000'000, 50'000'000, 75'000'000, 100'000'000,
		 250'000'000,// 500'000'000, 750'000'000, 1'000'000'000,
	};

	const int nops = 40'000'000;   // ops per test
	const int nt = 5;              // number of tests to average 

	for(double i=2.0; i<=20.0; i+=0.5) xs.push_back(i);
	for(double i=21; i<=30; i+=1) xs.push_back(i);

	std::string label;
	if (argc == 2)
		label = argv[1] + std::string("_");
	else
		label = "";

#ifdef SOA
	{
		for (auto b : bs) {
			std::ofstream f(label + "soa_amort_" + std::to_string(b));
			f << "\n----- graveyard structure of arrays ------------------------\n";
			f << "# ops, total mean, total median, mean time spent rb, mean time non-rb, window, rebuilds, a, x, n\n";
			for (auto x : xs)
				f << amorttester<graveyard_soa<>> (rng, x, b, nops, nt) << std::flush;
		}
	}
#endif

#ifdef AOS
	{
		for (auto b : bs) {
			std::ofstream f(label + "aos_amort_" + std::to_string(b));
			f << "\n----- graveyard array of structures ------------------------\n";
			f << "# ops, total mean, total median, mean time spent rb, mean time non-rb, window, rebuilds, a, x, n\n";
			for (auto x : xs)
				f << amorttester<graveyard_aos<>> (rng, x, b, nops, nt) << std::flush;
		}
	}
#endif
	cout << "Complete\n";

	return 0;
}

