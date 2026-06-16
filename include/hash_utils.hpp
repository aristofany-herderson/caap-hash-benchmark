#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <random>

namespace hp
{

  inline uint64_t splitmix64(uint64_t x)
  {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  inline uint64_t hash1(uint64_t key, uint64_t table_size)
  {
    return splitmix64(key * 0xD6E8FEB86659FD93ULL) % table_size;
  }

  inline uint64_t hash2(uint64_t key, uint64_t table_size)
  {
    return splitmix64(key * 0xA5A356AA556A5B77ULL + 0x9E3779B97F4A7C15ULL) % table_size;
  }

  inline size_t practical_block_size(size_t n, double alpha)
  {
    if (alpha >= 1.0)
      return 2;
    double log_n = std::log2(std::max((double)n, 4.0));
    double loglog = std::log2(std::max(log_n, 1.0));
    double scaled = (1.0 / (1.0 - alpha)) * loglog;
    return std::max(size_t(2), static_cast<size_t>(std::ceil(scaled)));
  }

  inline size_t block_index(size_t cell, size_t block_size)
  {
    return cell / block_size;
  }

  inline size_t block_start(size_t block_id, size_t block_size)
  {
    return block_id * block_size;
  }

  inline size_t cells_in_block(size_t block_id, size_t block_size, size_t table_size)
  {
    size_t start = block_start(block_id, block_size);
    if (start >= table_size)
      return 0;
    return std::min(block_size, table_size - start);
  }

  inline size_t num_blocks(size_t table_size, size_t block_size)
  {
    return (table_size + block_size - 1) / block_size;
  }

  inline bool coin_flip(std::mt19937_64 &rng)
  {
    return (rng() & 1ULL) != 0;
  }

}
