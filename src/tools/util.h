#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include "pcg_random.hpp"

double mean(std::vector<std::chrono::duration<double>>);
double median(std::vector<std::chrono::duration<double>>);
double mean(std::map<int,int>);
std::ostream& operator<<(std::ostream&,
                         const std::vector<std::chrono::duration<double>>&);
std::ostream& operator<<(std::ostream&, const std::vector<int>&);
void selsample(std::vector<uint32_t> *, int, uint32_t, pcg64 &);

#endif
