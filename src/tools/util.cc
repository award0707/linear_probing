#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include "util.h"

using std::chrono::duration;

double
mean(vector<duration<double> > v)
{
	return (std::reduce(v.begin(), v.end()) / v.size()).count();
}

double
median(vector<duration<double> > dv)
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
