#include <cmath>
#include "primes.h"

bool
is_prime(size_t n)
{
	if (n==2 || n==3) return true;
	if ((n%2)==0 || (n%3)==0) return false;

	for (size_t k=6; k < std::sqrt(n); k+=6)
	{
		if ((n % (k-1) == 0) || (n % (k+1) == 0))
			return false;
	}
	return true;
}

size_t
next_prime(size_t n)
{
	size_t m = (n/6) * 6;
	if (n==2) return 3;

	for(size_t km=m+5, kp=m+1; ; km+=6, kp+=6) {
		if (is_prime(kp) && kp>n) return kp;
		if (is_prime(km)) return km;
	}
}
