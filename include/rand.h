#ifndef __RAND
#define __RAND
/* A C-program for MT19937: Integer version (1998/4/6)            */
/*  genrand() generates one pseudorandom unsigned integer (32bit) */
/* which is uniformly distributed among 0 to 2^32-1  for each     */
/* call. sgenrand(seed) set initial values to the working area    */
/* of 624 words. Before genrand(), sgenrand(seed) must be         */
/* called once. (seed is any 32-bit integer except for 0).        */
/*   Coded by Takuji Nishimura, considering the suggestions by    */
/* Topher Cooper and Marc Rieffel in July-Aug. 1997.              */

/* This library is free software; you can redistribute it and/or   */
/* modify it under the terms of the GNU Library General Public     */
/* License as published by the Free Software Foundation; either    */
/* version 2 of the License, or (at your option) any later         */
/* version.                                                        */
/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of  */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.            */
/* See the GNU Library General Public License for more details.    */
/* You should have received a copy of the GNU Library General      */
/* Public License along with this library; if not, write to the    */
/* Free Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA   */
/* 02111-1307  USA                                                 */

/* Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.       */
/* When you use this, send an email to: matumoto@math.keio.ac.jp   */
/* with an appropriate reference to your work.                     */

/* REFERENCE                                                       */
/* M. Matsumoto and T. Nishimura,                                  */
/* "Mersenne Twister: A 623-Dimensionally Equidistributed Uniform  */
/* Pseudo-Random Number Generator",                                */
/* ACM Transactions on Modeling and Computer Simulation,           */
/* Vol. 8, No. 1, January 1998, pp 3--30.                          */

/* Period parameters */
#define __MT_N 624
#define __MT_M 397
#define __MT_MATRIX_A 0x9908b0df   /* constant vector a */
#define __MT_UPPER_MASK 0x80000000 /* most significant w-r bits */
#define __MT_LOWER_MASK 0x7fffffff /* least significant r bits */

/* Tempering parameters */
#define __MT_TEMPERING_MASK_B 0x9d2c5680
#define __MT_TEMPERING_MASK_C 0xefc60000
#define __MT_TEMPERING_SHIFT_U(y) (y >> 11)
#define __MT_TEMPERING_SHIFT_S(y) (y << 7)
#define __MT_TEMPERING_SHIFT_T(y) (y << 15)
#define __MT_TEMPERING_SHIFT_L(y) (y >> 18)

static unsigned long mt[__MT_N]; /* the array for the state vector  */
static int mti = __MT_N + 1;     /* mti==N+1 means mt[N] is not initialized */

void set_seed(unsigned long seed);
unsigned long get_rand(void);
unsigned long get_rand_in_range(unsigned long low, unsigned long high);

/* initializing the array with a NONZERO seed */
void set_seed(unsigned long seed) {
  /* setting initial seeds to mt[N] using         */
  /* the generator Line 25 of Table 1 in          */
  /* [KNUTH 1981, The Art of Computer Programming */
  /*    Vol. 2 (2nd Ed.), pp102]                  */
  mt[0] = seed & 0xffffffff;
  for (mti = 1; mti < __MT_N; mti++)
    mt[mti] = (69069 * mt[mti - 1]) & 0xffffffff;
}

unsigned long get_rand(void) {
  unsigned long y;
  static unsigned long mag01[2] = {0x0, __MT_MATRIX_A};
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if (mti >= __MT_N) { /* generate N words at one time */
    int kk;

    if (mti == __MT_N + 1) /* if sgenrand() has not been called, */
      set_seed(4357);      /* a default initial seed is used   */

    for (kk = 0; kk < __MT_N - __MT_M; kk++) {
      y = (mt[kk] & __MT_UPPER_MASK) | (mt[kk + 1] & __MT_LOWER_MASK);
      mt[kk] = mt[kk + __MT_M] ^ (y >> 1) ^ mag01[y & 0x1];
    }
    for (; kk < __MT_N - 1; kk++) {
      y = (mt[kk] & __MT_UPPER_MASK) | (mt[kk + 1] & __MT_LOWER_MASK);
      mt[kk] = mt[kk + (__MT_M - __MT_N)] ^ (y >> 1) ^ mag01[y & 0x1];
    }
    y = (mt[__MT_N - 1] & __MT_UPPER_MASK) | (mt[0] & __MT_LOWER_MASK);
    mt[__MT_N - 1] = mt[__MT_M - 1] ^ (y >> 1) ^ mag01[y & 0x1];

    mti = 0;
  }

  y = mt[mti++];
  y ^= __MT_TEMPERING_SHIFT_U(y);
  y ^= __MT_TEMPERING_SHIFT_S(y) & __MT_TEMPERING_MASK_B;
  y ^= __MT_TEMPERING_SHIFT_T(y) & __MT_TEMPERING_MASK_C;
  y ^= __MT_TEMPERING_SHIFT_L(y);

  return y;
}

unsigned long get_rand_in_range(unsigned long low, unsigned long high) {
  return low + get_rand() % (high - low + 1);
}

#endif // __RAND
