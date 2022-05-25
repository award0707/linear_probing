#include <iostream>
#include <sstream>
#include <algorithm>
#include <limits>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <queue>
#include <random>
#include "olinear.h"

struct stats_t {
	kvstore &kvs;
	std::chrono::duration<double> wct;
	std::size_t opcount;
};

// for readable comma-separated prints in verbose mode
class comma_numpunct : public std::numpunct<char> { 
	protected:
		virtual char do_thousands_sep() const { return ','; }
		virtual std::string do_grouping() const { return "\03"; }
};

std::string filesize(double size);
void display_stats(stats_t &stats, bool verbose);
void verbose_stats(kvstore &c);
void terse_stats(kvstore &c);
std::size_t next_prime(std::size_t n);
bool is_prime(std::size_t n);

int main(int argc, char **argv) {
	int pinsert = 34, pfind = 33, premove;
	double ploadfactor = 0.70;
	std::size_t ptable_size = 1'000'000, trials = 1'000'000;
	bool verbose = false;

	int p;
	while ((p = getopt(argc, argv, "i:f:r:l:b:v")) != -1) {
		switch(p) 
		{
		case 'i':
			pinsert = std::stoi(optarg);
			break;
		case 'f':
			pfind = std::stoi(optarg);
			break;
		case 'b':
			ptable_size = std::stoi(optarg)*1000;
			break;
		case 'l':		
			ploadfactor = std::stod(optarg);
			if (!(ploadfactor > 0 && ploadfactor < 1)) {
				std::cout << "Load factor must be between 0 and 1\n";
				exit(1);
			}
			break;
		case 'r':
			trials = std::stoi(optarg)*1000;
			break;
		case 'v':
			verbose = true;
			break;
		case '?':
			if (optopt == 'i' || optopt == 'f' || optopt == 'l' || optopt == 'b' || optopt == 'r')
				std::cout << "Option " << optopt << " takes an argument\n";
			else
				std::cout << "Unknown option supplied.\n"
					  << "Valid options are i [insert %], f [find %],\n" 
					  << "b [# of buckets], r [# of trials in thousands], l [load factor]\n"
					  << "v [verbose mode]\n";
			return 1;
		default:
			std::abort();
		}
	}
	premove = 100 - pinsert - pfind;

	std::size_t table_size = next_prime(ptable_size);
	kvstore c(table_size);
	c.set_max_load_factor(1.0F);	// disable the automatic resizing

	if (verbose) {
		std::locale comma_locale(std::locale(), new comma_numpunct());
		std::cout.imbue(comma_locale);
		std::cout << "Using i=" << pinsert << ", f=" << pfind << ", r=" << premove << ", table size=" 
			<< table_size << ", lf=" << ploadfactor << "\n\n";
	}

	//
	// initialization phase
	// generate random numbers and insert into the table. maintain a list of all keys inserted
	// load up the table to the target load factor
	//
	std::vector<std::size_t> keys;
	auto wctstart = std::chrono::system_clock::now();
	stats_t loadstats = { c, {}, 0 };
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> data(0,1'000'000'000L);
	std::uniform_int_distribution<std::mt19937::result_type> operation(0,99); 
	while(c.load_factor() < ploadfactor) {
		std::size_t k = data(rng);
		if (c.insert(k, k/2))
			keys.push_back(k);
		loadstats.opcount++;
	}
	loadstats.wct = std::chrono::system_clock::now() - wctstart;
	std::cout << "loading | ";
	display_stats(loadstats, verbose);

	//
	// test phase
	// randomly insert a new element, find, or remove a key according to the desired proportions
	//
	stats_t runstats = {c, {}, 0};
	c.reset_counts();
	wctstart = std::chrono::system_clock::now();
	for(std::size_t i=0; i<trials; ++i) {
		int which = operation(rng);
		std::size_t k, index = 0;

		if (which < pinsert) {
			do {
				if (c.bucket_c() == c.element_c()) {
					std::cerr << "OOPS: table filled, trial " << i << "\n";
					exit(1);
					break;
				}
				k = data(rng);
			} while	(!c.insert(k, 42));
			keys.push_back(k);
		} else {
			// pick a key to find or remove
			if (!keys.empty()) {
				std::uniform_int_distribution<> pick(0,keys.size()-1);
				index = pick(rng);
				k = keys[index]; 
			} else {
				std::cerr << "ran out of keys\n";
				break;
			}

			if (which < pinsert + pfind) 
				c.find(k); 
			else {
				if(c.remove(k)) {
					std::swap(keys[index],keys.back());
					keys.pop_back();
				}
			}
		}

		runstats.opcount++;
	}
	runstats.wct = std::chrono::system_clock::now() - wctstart;
	std::cout << "running | ";
	display_stats(runstats, verbose);

	return 0;
}


void display_stats(stats_t &stats, bool verbose) {
	if (verbose) {
		verbose_stats(stats.kvs);
		std::cout << "------ Timing statistics ------\n";
		std::cout << "Performed " << stats.opcount << " operations\n";
		std::cout << "Finished test in " << stats.wct.count() << " seconds [Wall Clock]\n" << std::endl;
	} else {
		terse_stats(stats.kvs);
		std::cout << stats.opcount << "," << stats.wct.count() << ",";
	}
}

void terse_stats(kvstore &c) {
	std::cout << c.bucket_c() << ',' << c.element_c() << ',' << c.load_factor() << ',' << c.rebuilds << ','; 
	std::cout << c.collision_c() << ',' << c.avg_misses() << ',' << c.longest_search << ',';
}

void verbose_stats(kvstore &c) {

	std::cout << "------ Table statistics ------\n";
	std::cout << "Buckets: " << c.bucket_c() << "\n";
	std::cout << "Elements: " << c.element_c() << "\n";
	std::cout << "Load factor: " << c.load_factor() << "\n";
	std::cout << "Rebuilds: " << c.rebuilds << "\n";
	
	std::cout << "------ Search statistics ------\n";
	std::cout << "Find collisions: " << c.collision_c() << "\n";
	std::cout << "Avg. miss/find: " << c.avg_misses() << "\n";
	std::cout << "Longest search: " << c.longest_search << "\n";
}

bool is_prime(std::size_t n) {
	if (n==2 || n==3) return true;
	if ((n%2)==0 || (n%3)==0) return false;

	for (int k=6; k < std::sqrt(n); k+=6)
	{
		if ((n % (k-1) == 0) || (n % (k+1) == 0))
			return false;
	}
	return true;
}

std::size_t next_prime(std::size_t n) {
	int m = (n/6) * 6;
	if (n==2) return 3;

	for(int km=m+5, kp=m+1; ; km+=6, kp+=6) {
		if (is_prime(kp)) return kp;
		if (is_prime(km)) return km;		
	}
}
