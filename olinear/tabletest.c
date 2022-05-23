#include <iostream>
#include "orderedlinear.h"

int main() {
	kvstore t;
	std::cout << "testing...\n";
	for(int i=0; i<100; i++) {
		if(i % 3) t.set(i,i*3);
	}
	t.reset_counts();
	for(int i=0; i<100; i++) {
		int g;
		if (!t.get(i,&g)) {
			std:: cout << "n/a ";
		}
		else {
			std::cout << g << " ";
		}
	}
	std::cout << "remove\n";
	std::cout << "\n";
	for(int i=0; i<100; i+=2) {
		t.remove(i);
	}
	for(int i=0; i<100; i++) {
		int g;
		if (!t.get(i,&g)) {
			std:: cout << "n/a ";
		}
		else {
			std::cout << g << " ";
		}
	}
	std::cout << "\n";

	std::cout << t.insert_c << " " << t.find_c << " " << t.remove_c << "\n";
	std::cout << "element: " << t.element_c() << ", bucket: " << t.bucket_c() << ", collision: " << t.collision_c() << "\n";
	std::cout << "longest search: " << t.longest_search << ", average misses: " << t.avg_misses() << "\n";
	return 0;
}
