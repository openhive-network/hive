/*
   BLAKE2 reference source code package - optimized C implementations

   Written in 2012 by Samuel Neves <sneves@dei.uc.pt>

   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   You should have received a copy of the CC0 Public Domain Dedication along with
   this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/
#pragma once
#ifndef __BLAKE2_H__
#define __BLAKE2_H__

#include <stddef.h>
#include <stdint.h>

#include <equihash/blake2-def.h>

#if defined(__cplusplus)
extern "C" {
#endif

  // Streaming API
  int blake2b_init( blake2b_state *S, const uint8_t outlen );
  int blake2b_init_key( blake2b_state *S, const uint8_t outlen, const void *key, const uint8_t keylen );
  int blake2b_init_param( blake2b_state *S, const blake2b_param *P );
  int blake2b_update( blake2b_state *S, const uint8_t *in, uint64_t inlen );
  int blake2b_final( blake2b_state *S, uint8_t *out, uint8_t outlen );

  // Simple API
  int blake2b( uint8_t *out, const void *in, const void *key, const uint8_t outlen, const uint64_t inlen, uint8_t keylen );

  static inline int blake2( uint8_t *out, const void *in, const void *key, const uint8_t outlen, const uint64_t inlen, uint8_t keylen )
  {
    return blake2b( out, in, key, outlen, inlen, keylen );
  }

#if defined(__cplusplus)
}
#endif

#endif

