#ifndef __GRAVEYARD
#define __GRAVEYARD

#include <cstdint>

enum node_status { FULL, EMPTY, DELETED };
enum optype { INSERT, FIND, REMOVE, REHASH };

struct entry {
	int key;
	int value;
};

struct hashnode {
	struct entry data;
	node_status status;
	hashnode() { status = EMPTY; }
};

static const std::size_t primes[51] =
{
  /* 0     */              5ul,
  /* 1     */              11ul, 
  /* 2     */              23ul, 
  /* 3     */              47ul, 
  /* 4     */              97ul, 
  /* 5     */              199ul, 
  /* 6     */              409ul, 
  /* 7     */              823ul, 
  /* 8     */              1741ul, 
  /* 9     */              3469ul, 
  /* 10    */              6949ul, 
  /* 11    */              14033ul, 
  /* 12    */              28411ul, 
  /* 13    */              57557ul, 
  /* 14    */              116731ul, 
  /* 15    */              236897ul,
  /* 16    */              480881ul, 
  /* 17    */              976369ul,
  /* 18    */              1982627ul, 
  /* 19    */              4026031ul,
  /* 20    */              8175383ul, 
  /* 21    */              16601593ul, 
  /* 22    */              33712729ul,
  /* 23    */              68460391ul, 
  /* 24    */              139022417ul, 
  /* 25    */              282312799ul, 
  /* 26    */              573292817ul, 
  /* 27    */              1164186217ul,
  /* 28    */              2364114217ul, 
  /* 29    */              4294967291ul,
  /* 30    */ (std::size_t)8589934583ull,
  /* 31    */ (std::size_t)17179869143ull,
  /* 32    */ (std::size_t)34359738337ull,
  /* 33    */ (std::size_t)68719476731ull,
  /* 34    */ (std::size_t)137438953447ull,
  /* 35    */ (std::size_t)274877906899ull,
  /* 36    */ (std::size_t)549755813881ull,
  /* 37    */ (std::size_t)1099511627689ull,
  /* 38    */ (std::size_t)2199023255531ull,
  /* 39    */ (std::size_t)4398046511093ull,
  /* 40    */ (std::size_t)8796093022151ull,
  /* 41    */ (std::size_t)17592186044399ull,
  /* 42    */ (std::size_t)35184372088777ull,
  /* 43    */ (std::size_t)70368744177643ull,
  /* 44    */ (std::size_t)140737488355213ull,
  /* 45    */ (std::size_t)281474976710597ull,
  /* 46    */ (std::size_t)562949953421231ull, 
  /* 47    */ (std::size_t)1125899906842597ull,
  /* 48    */ (std::size_t)2251799813685119ull, 
  /* 49    */ (std::size_t)4503599627370449ull,
  /* 50    */ (std::size_t)9007199254740881ull, 
};

class kvstore {
	private:
		hashnode *table;
		std::size_t buckets;
		std::size_t elements;
		int prime_index;
		double p_max_load_factor;
		int rebuild_window;

		int search_count;
		double avg_find_miss;
		int coll;

		bool insert(hashnode *t, int key, int value, bool rehashing);
		uint64_t hash(int);
		void update_misses(int misses, enum optype op);
		void rebuild();

	public:
		kvstore(int b = 7);
		~kvstore();

		bool get(int key, int* value);
	        bool set(int key, int value);

		bool insert(int key, int value);
		int* find(int key);
		bool remove(int key);
		void rehash();
		void set_max_load_factor(double f);
		
		// Performance characteristics
		int insert_c, find_c, remove_c;
		int insert_coll_c, find_coll_c, remove_coll_c;
		int rehashing_insert_c, rehashing_insert_coll_c;
		int failed_insert, failed_find, failed_remove;
		int rehashes;
		int rebuilds;
		int longest_search;

		void reset_counts();
		double load_factor();
		double avg_misses();
		int bucket_c();
		int collision_c();
		int element_c();
};

#endif
