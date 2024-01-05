#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include <numeric>
#include "util.h"

using std::chrono::duration;
using std::vector;

double
mean(vector<duration<double>> v)
{
	return (std::reduce(v.begin(), v.end()) / v.size()).count();
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
