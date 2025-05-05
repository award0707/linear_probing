#include <iostream>
#include <string>
#include <sched.h>

#include "pcg_random.hpp"
#include "primes.h"
#include "util.h"

#include "testers/amorttester.hpp"
#include "graveyard.h"
#include "ordered.h"
#include "linear.h"

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

int main(int argc, char **argv)
{
	vector<double> xs;
	const vector<uint64_t> bs { // 10'000,
		 /* 25'000, 50'000, 75'000, 100'000, */
		 1'000'000, 
		 /* 2'500'000, 5'000'000, 7'500'000, 10'000'000, */
		 /* 25'000'000, 50'000'000, 75'000'000, 100'000'000, */
		 /* 250'000'000, 500'000'000, 750'000'000, */
	};

	const int nops = 10'000'000;   // ops per test
	const int nt = 10;              // number of tests to average 

	for(double i=6; i<=25; i+=0.2) xs.push_back(i);
	//for(double i=21; i<=30; i+=1) xs.push_back(i);

	std::string label;
	if (argc == 2)
		label = argv[1];
	else
		label = "";


	// AOS X
	{
		std::ofstream f(label + "aos_amort_x");
		f << "\n----- graveyard, array of structures, by x [core " << sched_getcpu() << "]\n";
		f << "ops per test, x, n, total mean time, total median time, total time, "
		     "mean ops time, total ops time, mean rb time, total rb time, "
		     "rb window, rebuilds, lf\n";
		for (auto x : xs) {
			for (auto b : bs)
				f << amorttester<graveyard_aos<>> (rng, x, b, nops, nt) << std::flush;
		}
	}
	// SOA X
	{
		std::ofstream f(label + "soa_amort_x");
		f << "\n----- graveyard, structure of arrays, by x, [core " << sched_getcpu() << "]\n";
		f << "ops per test, x, n, total mean time, total median time, total time, "
		     "mean ops time, total ops time, mean rb time, total rb time, "
		     "rb window, rebuilds, lf\n";
		for (auto x : xs) {
			for (auto b : bs)
				f << amorttester<graveyard_soa<>> (rng, x, b, nops, nt) << std::flush;
		}
	}

	/* // AOS B */
	/* { */
	/* 	std::ofstream f(label + "aos_amort_b"); */
	/* 	f << "\n----- graveyard, array of structures, by b [core " << sched_getcpu() << "]\n"; */
	/* 	f << "ops per test, x, n, total mean time, total median time, total time, " */
	/* 	     "mean ops time, total ops time, mean rb time, total rb time, " */
	/* 	     "rb window, rebuilds, lf\n"; */
	/* 	for (auto b : bs) { */
	/* 		for (auto x : xs) */
	/* 			f << amorttester<graveyard_aos<>> (rng, x, b, nops, nt) << std::flush; */
	/* 	} */
	/* } */

	/* // SOA B */
	/* { */
	/* 	std::ofstream f(label + "soa_amort_b"); */
	/* 	f << "\n----- graveyard, structure of arrays, by b, [core " << sched_getcpu() << "]\n"; */
	/* 	f << "ops per test, x, n, total mean time, total median time, total time, " */
	/* 	     "mean ops time, total ops time, mean rb time, total rb time, " */
	/* 	     "rb window, rebuilds, lf\n"; */
	/* 	for (auto b : bs) { */
	/* 		for (auto x : xs) */
	/* 			f << amorttester<graveyard_soa<>> (rng, x, b, nops, nt) << std::flush; */
	/* 	} */
	/* } */

	cout << "Complete\n";

	return 0;
}

