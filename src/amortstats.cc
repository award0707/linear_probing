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
#define SOA

int main(int argc, char **argv)
{
	const vector<int> xs { 20 };
	const vector<uint64_t> bs { 10'000,
		 20'000, 40'000, 60'000, 80'000, 100'000,
	         200'000, 400'000, 600'000, 800'000, 1'000'000,
	         2'000'000, 4'000'000, 6'000'000, 8'000'000, 10'000'000,
		 20'000'000, 40'000'000, 60'000'000, 80'000'000, 100'000'000,
		 //200'000'000, 400'000'000, 600'000'000, 800'000'000, 1'000'000'000,
	};

	const int nops = 10'000'000;
	const int nt = 5;              // number of tests to average overk

	std::string label;
	if (argc == 2)
		label = argv[1] + std::string("_");
	else
		label = "";

#ifdef SOA
	{
		for (auto x : xs) {
			std::ofstream f(label + "soa_amort_" + std::to_string(x));
			f << "\n----- graveyard_soa --------------------------------\n";
			f << "# ops, mean, median, RW, RB, a, x, n\n";
			for (auto b : bs)
				f << amorttester<graveyard_soa<>> (rng, x, b, nops, nt);
		}
	}
#endif

#ifdef AOS
	{
		for (auto x : xs) {
			std::ofstream f(label + "aos_amort_" + std::to_string(x));
			f << "\n----- graveyard_aos --------------------------------\n";
			f << "# ops, mean, median, RW, RB, a, x, n\n";
			for (auto b : bs)
				f << amorttester<graveyard_aos<>> (rng, x, b, nops, nt);
		}
	}
#endif
	cout << "Complete\n";

	return 0;
}

