#include <iostream>
#include <fstream>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <random>

#include "pcg_random.hpp"
#include "testers/loadtester.hpp"
#include "hashtables/graveyard.h"
#include "hashtables/ordered.h"
#include "hashtables/linear.h"

pcg_extras::seed_seq_from<std::random_device> seed_source;
pcg64 rng(seed_source);

int main(int argc, char **argv)
{
	const int x = 1000;      // load factor 1-1/x
	const int i = 100;       // # of data points for profiling
	std::vector<uint64_t> ns = {
//		10'000'000,
//		25'000'000,
//		50'000'000,
//		100'000'000,
//		250'000'000,
//		500'000'000,
//		750'000'000,
		1'000'000'000,
	};

	// array of structures
	{ std::ofstream f("loadbench_graveyard_aos");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<graveyard_aos<>>(rng,n,x,i,true); }

	{ std::ofstream f("loadbench_ordered_aos");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<ordered_aos<>>(rng,n,x,i,true); }

	{ std::ofstream f("loadbench_linear_aos");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<linear_aos<>>(rng,n,x,i,true); }

	// structure of arrays
	{ std::ofstream f("loadbench_graveyard_soa");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<graveyard_soa<>>(rng,n,x,i,true); }

	{ std::ofstream f("loadbench_ordered_soa");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<ordered_soa<>>(rng,n,x,i,true); }

	{ std::ofstream f("loadbench_linear_soa");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<linear_soa<>>(rng,n,x,i,true); }

	// aos, no rebuilds during build
	{ std::ofstream f("loadbench_graveyard_aos_norebuild");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<graveyard_aos<>>(rng,n,x,i,false); }

	{ std::ofstream f("loadbench_ordered_aos_norebuild");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<ordered_aos<>>(rng,n,x,i,false); }

	{ std::ofstream f("loadbench_linear_aos_norebuild");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<linear_aos<>>(rng,n,x,i,false); }

	// soa, no rebuilds during build
	{ std::ofstream f("loadbench_graveyard_soa_norebuild");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<graveyard_soa<>>(rng,n,x,i,false); }

	{ std::ofstream f("loadbench_ordered_soa_norebuild");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<ordered_soa<>>(rng,n,x,i,false); }

	{ std::ofstream f("loadbench_linear_soa_norebuild");
	  for (auto n : ns)
		f << "------------------------------------------\n"
		  << loadtester<linear_soa<>>(rng,n,x,i,false); }

	return 0;
}

