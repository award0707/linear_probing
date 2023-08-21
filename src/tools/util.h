#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <chrono>

using std::vector;
using std::chrono::duration;

double mean(vector<duration<double> >);
double median(vector<duration<double> >);
std::ostream& operator<<(std::ostream&, const vector<duration<double> >&);

#endif
