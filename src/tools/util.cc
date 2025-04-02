#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <chrono>
#include <random>
#include <numeric>
#include "util.h"
#include "pcg_random.hpp"

using std::chrono::duration;
using std::vector;

double
mean(vector<duration<double>> v)
{
	return (std::reduce(v.begin(), v.end()) / v.size()).count();
}

double
sum(vector<duration<double>> v)
{
	return (std::reduce(v.begin(), v.end())).count();
}

double
mean(std::map<int,int> m)
{
	int count=0, sum=0;
	for (std::map<int,int>::const_iterator it = m.begin();
	     it != m.end();
	     ++it) {
		sum += it->first * it->second;
		count += it->second;
	}

	return sum/(double)count;
}

double
median(vector<duration<double>> dv)
{
	double med;
	vector<double> v;

	for (auto i : dv) v.push_back(i.count());
	int n = v.size() / 2.0;

	std::nth_element(v.begin(), v.begin() + n, v.end());
	med = v[n];

	if ((v.size() % 2) == 0) {
		auto max = std::max_element(v.begin(), v.begin() + n);
		med = (*max + med) / 2.0;
	}

	return med;
}

std::ostream&
operator<<(std::ostream& os, const std::vector<duration<double> >& v)
{
	os << '[';
	for (auto &i : v)
		os << i.count() << ' ';
	os << ']';
	return os;
}


std::ostream& operator<<(std::ostream& os, const std::vector<int>& v)
{
	os << '[';
	for (auto &i : v)
		os << i << ' ';
	os << ']';
	return os;
}

// push n samples from [0,N] without replacement onto vector v
void
selsample(vector<uint32_t> *v, int n, uint32_t N, pcg64 &rng)
{
	// J.S. Vitter. An efficient algorithm for sequential random sampling.
	// RR-0624, INRIA. 1987. inria-00075929

	using std::exp, std::log;
	std::uniform_real_distribution<>
	       U(0, std::nextafter(1.0, std::numeric_limits<double>::max()));

	double nreal = n,
	       ninv = 1.0/nreal,
	       Nreal = N,
	       Vprime = exp(log(U(rng)) * ninv),
	       qu1real = -nreal + 1.0 + Nreal,
	       X, V, y1, y2, negSreal, nmin1inv, top, bottom;
	int64_t qu1 = -n + 1 + N,
	        negalphainv = -13,
		sel = -1,
		threshold = -negalphainv * n,
		S, limit;

	// method d 
	while (n > 1 && threshold < N) {
		nmin1inv = 1.0 / (-1.0 + nreal);
		while(1) {
			// step D2: generate U and X
			while (1) {
				X = Nreal * (-Vprime + 1.0);
				S = (int)X;
				if (S < qu1) break;
				Vprime = exp(log(U(rng)) * ninv);
			}
			V = U(rng);
			negSreal = -S;

			// step D3: accept?
			y1 = exp(log(V * Nreal/qu1real) * nmin1inv);
			Vprime = y1 * (-X/Nreal + 1.0)
			        * qu1real/(negSreal+qu1real);
			if (Vprime <= 1.0) break; // test 2.8 true

			// step D4: accept?
			y2 = 1.0;
			top = -1.0 + Nreal;
			if (-1 + n > S) {
				bottom = -nreal + Nreal;
				limit = -S + N;
			} else {
				bottom = -1.0 + negSreal + Nreal;
				limit = qu1;
			}
			for (int64_t t = -1 + N; t >= limit; --t) {
				y2 = (y2 * top) / bottom;
				top = -1.0 + top;
				bottom = -1.0 + bottom;
			}
			if (Nreal / (-X + Nreal) >= y1 * exp(log(y2) * nmin1inv)) {
				// accept
				Vprime = exp(log(U(rng)) * nmin1inv);
				break;
			}
			Vprime = exp(log(U(rng)) * ninv);
		}
		// step D5
		sel += S + 1;
		v->push_back(sel);
		N = -S + (-1 + N);
		Nreal = negSreal + (-1.0 + Nreal);
		n = -1 + n;
		nreal = -1.0 + nreal;
		ninv = nmin1inv;
		qu1 = -S + qu1;
		qu1real = negSreal + qu1real;
		threshold += negalphainv;
	}
	if (n > 1) { // switch to method A if any left
		double top = N - n, quot;
		Nreal = N;

		while (n >= 2) {
			S = 0;
			V = U(rng);
			quot = top/Nreal;
			while (quot > V) {
				S = S + 1;
				top = -1.0 + top;
				Nreal = -1.0 + Nreal;
				quot = (quot*top)/Nreal;
			}
			sel += S+1;
			v->push_back(sel);
			Nreal = -1.0 + Nreal;
			n = -1 + n;
		}
		if (n == 1) {
			S = (int)((int)(Nreal+0.5) * U(rng));
			sel += S+1;
			v->push_back(sel);
		}
	} else if (n == 1) { // just in case of rounding error
		S = (int)(N * Vprime);
		sel += S+1;
		v->push_back(sel);
	}
}
