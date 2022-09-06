#include <iostream>
#include <vector>
#include <map>
#include "hashtable.h"
#include "primes.h"

using std::cerr;

hashtable::hashtable(uint64_t b)
{
	prime_index = 0;
	while(b > primes[prime_index]) {
		prime_index++;
	}
	
	table = new record[b];
	if (!table) { 
		cerr << "Couldn't allocate\n";
	}
	for(uint64_t i=0; i<b; i++) {
		table[i].state = EMPTY;
	}

	buckets = b;
	records = 0;
	max_load_factor = 0.5;
	miss_running_avg = 0;
	search_count = 0;
	total_misses = 0;

	reset_perf_counts();
}

hashtable::~hashtable()
{
	delete[] table;	
}

uint64_t
hashtable::hash(int k)
{
	int r = k % buckets;
	return (r<0)?r+buckets:r;	
}

void
hashtable::resize(uint64_t b)
{
	uint64_t oldbuckets = buckets;
	cerr << "resize(): rehashing into " << b << " buckets\n";
	
	record *newtable = new record[b];
	for(uint64_t i=0; i<b; ++i) {
		newtable[i].state = EMPTY;
	}
	records = 0;
	buckets = b;

	for(uint64_t i=0; i<oldbuckets; ++i) {
		if (table[i].state == FULL)
			insert(newtable, table[i].key, table[i].value, true);
	}

	delete[] table;
	table = newtable;
	resizes++;	
}

void
hashtable::set_max_load_factor(double f)
{
	max_load_factor = f;
}

void
hashtable::update_misses(int misses, enum optype op)
{
	int n = ++search_count;
	total_misses += misses;
	switch(op) {
		case INSERT:
			insert_misses += misses; break;
		case QUERY:
			query_misses += misses; break;
		case REMOVE:
			remove_misses += misses; break;
		case REBUILD:
			rebuild_insert_misses += misses; break;
	}
	if (misses > longest_search) longest_search = misses;
	miss_running_avg = miss_running_avg * (double)(n-1)/n + (double)misses/n;
}

void
hashtable::reset_perf_counts()
{
	inserts = queries = removes = rebuild_inserts = 0;
	insert_misses = query_misses = remove_misses = rebuild_insert_misses = 0;
	failed_inserts = failed_removes = failed_queries = duplicates = 0;
	longest_search = 0;
	total_misses = 0;
	miss_running_avg = 0;
	search_count = 0;
	resizes = 0;
}

void
hashtable::report_testing_stats(std::ostream &os)
{
	os << "Misses\n";
	os << "Insert: " << insert_misses;
	if (inserts)
		os << ", " << (double)insert_misses/inserts << " miss/insert";
	os << "\n";
	
	os << "Query: " << query_misses;
    if (queries)
		os << ", " << (double)query_misses/queries << " miss/query";
	os << "\n";
	
	os << "Remove: " << remove_misses;
	if (removes)
			os << ", " << (double)remove_misses/removes << " miss/remove";
	os << "\n";

	os << "Total: " << total_misses
		<< ", " << (double)total_misses/(inserts+queries+removes)
		<< " miss/op\n";

	os << "Fails\nInserts: " << failed_inserts
			<< " (" << duplicates << " dup) / " << inserts
	        << ", Queries: " << failed_queries << " / " << queries 
	        << ", Removes: " << failed_removes << " / " << removes << "\n";	
}

void
hashtable::cluster_freq(std::map<int,int> &clust,
						std::map<int,int> &clust_tombs)
{
	int cs = 0, csi = 0;	// cluster size, clusters size including tombs
	int t=0;
	for (int i=0; i<buckets; ++i) {
		switch(table[i].state) {
			case FULL:
				++cs;
				++csi;
				break;
			case EMPTY:
				if (cs) clust[cs]++;
				if (csi) clust_tombs[csi]++;
				cs = csi = 0;
				break;
			case DELETED:
				++csi;
				if (cs) clust[cs]++;
				cs = 0;
				++t;
				break;
		}
	}
	if (cs) clust[cs]++;
	if (csi) clust_tombs[csi]++;

	int sum=0, tsum=0;
	 for(auto it = clust.begin(); it != clust.end(); ++it)
		  sum += it->first * it->second;
	cerr << "sum=" << sum << ",records=" << records << "\n";
	 for(auto it = clust_tombs.begin(); it != clust_tombs.end(); ++it)
		  tsum += it->first * it->second;
	 cerr << "tsum=" <<tsum<<",tsum-t="<<tsum-t<<",records="<<records<<"\n";
}

void
hashtable::cluster_len(std::vector<int> &clust,
					   std::vector<int> &clust_tombs)
{
	int cs = 0, csi = 0;	// cluster size, cluster size including tombs
	int t=0;
	for (int i=0; i<buckets; ++i) {
		switch(table[i].state) {
			case FULL:
				++cs;
				++csi;
				break;
			case EMPTY:
				if (cs) clust.push_back(cs);
				if (csi) clust_tombs.push_back(csi);
				cs = csi = 0;
				break;
			case DELETED:
				++csi;
				if (cs) clust.push_back(cs);
				cs = 0;
				++t;
				break;
		}
	}
	if (cs) clust.push_back(cs);
	if (csi) clust_tombs.push_back(csi);

	int sum=0, tsum=0;

	 for(auto it = clust.begin(); it != clust.end(); ++it)
		  sum += *it;
	cerr << "sum=" << sum << ",records=" << records << "\n";
	 for(auto it = clust_tombs.begin(); it != clust_tombs.end(); ++it)
		  tsum += *it;
	 cerr << "tsum=" <<tsum<<",tsum-t="<<tsum-t<<",records="<<records<<"\n";
}

double
hashtable::load_factor()
{
	return (double)records/buckets;
}

double
hashtable::avg_misses()
{
	return miss_running_avg;
}

uint64_t
hashtable::table_size()
{
	return buckets;
}

uint64_t
hashtable::table_size_bytes()
{
	return buckets*sizeof(record);
}

uint64_t
hashtable::num_records()
{
	return records;
}

