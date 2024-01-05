#include "util.h"
#include <iostream>
#include <map>

using std::map;

int main() {
	std::map<int,int> m = {
	     { 0, 50 },
	     { 1, 18 },
	     { 2, 5  },
	     { 3, 1  },
	     { 6, 1  },
	     { 9, 1  }};

	int s=0,c=0;
	for (std::map<int,int>::const_iterator it = m.begin();
	     it != m.end();
	     ++it) {
		std::cout << it->first << ": " << it->second << "\n";
		s += it->first*it->second;
		c += it->second;
		std::cout << "So far " << s << ", out of " << c << "counted\n";
	}

	std::cout << "Average: " << mean(m);

	return 0;
	     
}
