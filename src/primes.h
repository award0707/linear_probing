#ifndef PRIMES_H
#define PRIMES_H

#include <cstddef>

// just some prime number support functions

std::size_t next_prime(size_t n);
bool is_prime(size_t n);

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

#endif
