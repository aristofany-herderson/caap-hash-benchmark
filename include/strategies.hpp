#pragma once

#include "hash_table.hpp"
#include "hash_utils.hpp"

#include <algorithm>
#include <limits>
#include <random>
#include <vector>

namespace hp
{

  constexpr uint64_t kEmptyKey = std::numeric_limits<uint64_t>::max();

  class LinearProbingTable : public HashTableStrategy
  {
  public:
    explicit LinearProbingTable(size_t capacity)
        : table_(capacity, kEmptyKey), capacity_(capacity) {}

    bool insert(uint64_t key) override
    {
      if (key == kEmptyKey)
        return false;
      uint64_t probes = 0;
      size_t idx = static_cast<size_t>(hash1(key, capacity_));
      while (true)
      {
        ++probes;
        if (table_[idx] == kEmptyKey)
        {
          table_[idx] = key;
          ++size_;
          metrics_.insert_stats.record(probes);
          return true;
        }
        if (table_[idx] == key)
        {
          metrics_.insert_stats.record(probes);
          return false;
        }
        idx = (idx + 1) % capacity_;
      }
    }

    bool search(uint64_t key) const override
    {
      uint64_t probes = 0;
      size_t idx = static_cast<size_t>(hash1(key, capacity_));
      while (true)
      {
        ++probes;
        if (table_[idx] == kEmptyKey)
        {
          const_cast<LinearProbingTable *>(this)->metrics_.search_fail_stats.record(probes);
          return false;
        }
        if (table_[idx] == key)
        {
          const_cast<LinearProbingTable *>(this)->metrics_.search_success_stats.record(probes);
          return true;
        }
        idx = (idx + 1) % capacity_;
      }
    }

    void clear() override
    {
      std::fill(table_.begin(), table_.end(), kEmptyKey);
      size_ = 0;
      metrics_.reset_runtime();
      metrics_.max_cluster = 0;
    }

    size_t size() const override { return size_; }
    size_t capacity() const override { return capacity_; }
    double load_factor() const override
    {
      return capacity_ == 0 ? 0.0 : static_cast<double>(size_) / static_cast<double>(capacity_);
    }

    void reset_metrics() override { metrics_.reset_runtime(); }
    const TableMetrics &metrics() const override { return metrics_; }
    void refresh_cluster_metric() override
    {
      metrics_.max_cluster = compute_max_cluster(table_.data(), capacity_, kEmptyKey);
    }

    std::string name() const override { return "LinearProbing"; }
    std::string description() const override { return "Sondagem linear classica (baseline)."; }

  private:
    std::vector<uint64_t> table_;
    size_t capacity_;
    size_t size_ = 0;
    mutable TableMetrics metrics_;
  };

  class LocallyLinearTable : public HashTableStrategy
  {
  public:
    LocallyLinearTable(size_t capacity, double target_alpha)
        : table_(capacity, kEmptyKey),
          capacity_(capacity),
          block_size_(practical_block_size(capacity, target_alpha)),
          block_loads_(num_blocks(capacity, block_size_), 0),
          rng_(0xC0FFEEULL)
    {
      metrics_.auxiliary_memory_bytes = block_loads_.size() * sizeof(size_t);
    }

    bool insert(uint64_t key) override
    {
      if (key == kEmptyKey)
        return false;
      size_t h1 = static_cast<size_t>(hash1(key, capacity_));
      size_t h2 = static_cast<size_t>(hash2(key, capacity_));
      size_t b1 = block_index(h1, block_size_);
      size_t b2 = block_index(h2, block_size_);

      size_t chosen_block = b1;
      if (block_loads_[b1] > block_loads_[b2])
        chosen_block = b2;
      else if (block_loads_[b1] == block_loads_[b2] && b1 != b2 && coin_flip(rng_))
        chosen_block = b2;

      size_t start_cell = (chosen_block == b1) ? h1 : h2;
      uint64_t probes = 0;
      size_t pos = insert_in_block(start_cell, key, probes);
      if (pos == capacity_)
        return false;

      table_[pos] = key;
      ++size_;
      ++block_loads_[block_index(pos, block_size_)];
      metrics_.insert_stats.record(probes);
      return true;
    }

    bool search(uint64_t key) const override
    {
      uint64_t probes = 0;
      size_t h1 = static_cast<size_t>(hash1(key, capacity_));
      size_t h2 = static_cast<size_t>(hash2(key, capacity_));

      SearchOutcome o1 = search_from_cell(h1, key, probes);
      if (o1 == SearchOutcome::Found)
      {
        const_cast<LocallyLinearTable *>(this)->metrics_.search_success_stats.record(probes);
        return true;
      }
      SearchOutcome o2 = SearchOutcome::BlockFull;
      if (h2 != h1)
      {
        o2 = search_from_cell(h2, key, probes);
        if (o2 == SearchOutcome::Found)
        {
          const_cast<LocallyLinearTable *>(this)->metrics_.search_success_stats.record(probes);
          return true;
        }
      }
      if (o1 == SearchOutcome::EmptyGap || o2 == SearchOutcome::EmptyGap)
      {
        const_cast<LocallyLinearTable *>(this)->metrics_.search_fail_stats.record(probes);
        return false;
      }
      size_t b1 = block_index(h1, block_size_);
      size_t b2 = block_index(h2, block_size_);
      size_t block_count = num_blocks(capacity_, block_size_);
      for (size_t round = 1; round < block_count; ++round)
      {
        for (size_t seed_block : {b1, b2})
        {
          size_t bid = (seed_block + round) % block_count;
          SearchOutcome outcome = search_block_from_start(bid, key, probes);
          if (outcome == SearchOutcome::Found)
          {
            const_cast<LocallyLinearTable *>(this)->metrics_.search_success_stats.record(probes);
            return true;
          }
          if (outcome == SearchOutcome::EmptyGap)
          {
            const_cast<LocallyLinearTable *>(this)->metrics_.search_fail_stats.record(probes);
            return false;
          }
        }
      }
      const_cast<LocallyLinearTable *>(this)->metrics_.search_fail_stats.record(probes);
      return false;
    }

    void clear() override
    {
      std::fill(table_.begin(), table_.end(), kEmptyKey);
      std::fill(block_loads_.begin(), block_loads_.end(), 0);
      size_ = 0;
      metrics_.reset_runtime();
      metrics_.max_cluster = 0;
    }

    size_t size() const override { return size_; }
    size_t capacity() const override { return capacity_; }
    double load_factor() const override
    {
      return capacity_ == 0 ? 0.0 : static_cast<double>(size_) / static_cast<double>(capacity_);
    }

    void reset_metrics() override { metrics_.reset_runtime(); }
    const TableMetrics &metrics() const override { return metrics_; }
    void refresh_cluster_metric() override
    {
      metrics_.max_cluster = compute_max_cluster(table_.data(), capacity_, kEmptyKey);
    }

    std::string name() const override { return "LocallyLinear"; }
    std::string description() const override
    {
      return "Two-way locally-linear probing (Dalal et al., 2023).";
    }

  private:
    enum class SearchOutcome
    {
      Found,
      EmptyGap,
      BlockFull
    };

    size_t insert_in_block(size_t start_cell, uint64_t /*key*/, uint64_t &probes)
    {
      size_t bid = block_index(start_cell, block_size_);
      size_t block_count = num_blocks(capacity_, block_size_);
      for (size_t round = 0; round < block_count; ++round)
      {
        size_t cb = (bid + round) % block_count;
        size_t cells = cells_in_block(cb, block_size_, capacity_);
        if (block_loads_[cb] >= cells)
          continue;
        size_t block_begin = block_start(cb, block_size_);
        size_t offset = (round == 0) ? (start_cell - block_begin) : 0;
        for (size_t i = 0; i < cells; ++i)
        {
          size_t pos = block_begin + ((offset + i) % cells);
          ++probes;
          if (table_[pos] == kEmptyKey)
            return pos;
        }
      }
      return capacity_;
    }

    SearchOutcome search_from_cell(size_t start_cell, uint64_t key, uint64_t &probes) const
    {
      size_t bid = block_index(start_cell, block_size_);
      size_t cells = cells_in_block(bid, block_size_, capacity_);
      size_t block_begin = block_start(bid, block_size_);
      size_t offset = start_cell - block_begin;
      bool saw_empty = false;
      for (size_t i = 0; i < cells; ++i)
      {
        size_t pos = block_begin + ((offset + i) % cells);
        ++probes;
        if (table_[pos] == kEmptyKey)
        {
          saw_empty = true;
          break;
        }
        if (table_[pos] == key)
          return SearchOutcome::Found;
      }
      if (saw_empty)
        return SearchOutcome::EmptyGap;
      if (block_loads_[bid] < cells)
        return SearchOutcome::EmptyGap;
      return SearchOutcome::BlockFull;
    }

    SearchOutcome search_block_from_start(size_t bid, uint64_t key, uint64_t &probes) const
    {
      return search_from_cell(block_start(bid, block_size_), key, probes);
    }

    std::vector<uint64_t> table_;
    size_t capacity_, block_size_;
    std::vector<size_t> block_loads_;
    size_t size_ = 0;
    mutable TableMetrics metrics_;
    mutable std::mt19937_64 rng_;
  };

  class WalkFirstTable : public HashTableStrategy
  {
  public:
    WalkFirstTable(size_t capacity, double target_alpha)
        : table_(capacity, kEmptyKey),
          capacity_(capacity),
          block_size_(practical_block_size(capacity, target_alpha)),
          block_loads_(num_blocks(capacity, block_size_), 0),
          rng_(0xFACEFEEDULL)
    {
      metrics_.auxiliary_memory_bytes = block_loads_.size() * sizeof(size_t);
    }

    bool insert(uint64_t key) override
    {
      if (key == kEmptyKey)
        return false;
      size_t h1 = static_cast<size_t>(hash1(key, capacity_));
      size_t h2 = static_cast<size_t>(hash2(key, capacity_));
      uint64_t p1 = 0, p2 = 0;
      size_t u = terminal_empty(h1, p1);
      size_t v = terminal_empty(h2, p2);
      size_t bu = block_index(u, block_size_);
      size_t bv = block_index(v, block_size_);
      size_t chosen = u;
      if (block_loads_[bu] > block_loads_[bv])
        chosen = v;
      else if (block_loads_[bu] == block_loads_[bv] && u != v && coin_flip(rng_))
        chosen = v;
      table_[chosen] = key;
      ++size_;
      ++block_loads_[block_index(chosen, block_size_)];
      metrics_.insert_stats.record(p1 + p2);
      return true;
    }

    bool search(uint64_t key) const override
    {
      uint64_t probes = 0;
      size_t idx1 = static_cast<size_t>(hash1(key, capacity_));
      size_t idx2 = static_cast<size_t>(hash2(key, capacity_));
      bool a1 = true, a2 = true;
      while (a1 || a2)
      {
        if (a1)
        {
          ++probes;
          if (table_[idx1] == key)
          {
            const_cast<WalkFirstTable *>(this)->metrics_.search_success_stats.record(probes);
            return true;
          }
          if (table_[idx1] == kEmptyKey)
            a1 = false;
          else
            idx1 = (idx1 + 1) % capacity_;
        }
        if (a2)
        {
          ++probes;
          if (table_[idx2] == key)
          {
            const_cast<WalkFirstTable *>(this)->metrics_.search_success_stats.record(probes);
            return true;
          }
          if (table_[idx2] == kEmptyKey)
            a2 = false;
          else
            idx2 = (idx2 + 1) % capacity_;
        }
      }
      const_cast<WalkFirstTable *>(this)->metrics_.search_fail_stats.record(probes);
      return false;
    }

    void clear() override
    {
      std::fill(table_.begin(), table_.end(), kEmptyKey);
      std::fill(block_loads_.begin(), block_loads_.end(), 0);
      size_ = 0;
      metrics_.reset_runtime();
      metrics_.max_cluster = 0;
    }

    size_t size() const override { return size_; }
    size_t capacity() const override { return capacity_; }
    double load_factor() const override
    {
      return capacity_ == 0 ? 0.0 : static_cast<double>(size_) / static_cast<double>(capacity_);
    }

    void reset_metrics() override { metrics_.reset_runtime(); }
    const TableMetrics &metrics() const override { return metrics_; }
    void refresh_cluster_metric() override
    {
      metrics_.max_cluster = compute_max_cluster(table_.data(), capacity_, kEmptyKey);
    }

    std::string name() const override { return "WalkFirst"; }
    std::string description() const override
    {
      return "Two-way post-linear probing com escolha pelo bloco menos carregado.";
    }

  private:
    size_t terminal_empty(size_t start, uint64_t &probes) const
    {
      size_t idx = start;
      while (true)
      {
        ++probes;
        if (table_[idx] == kEmptyKey)
          return idx;
        idx = (idx + 1) % capacity_;
      }
    }

    std::vector<uint64_t> table_;
    size_t capacity_, block_size_;
    std::vector<size_t> block_loads_;
    size_t size_ = 0;
    mutable TableMetrics metrics_;
    mutable std::mt19937_64 rng_;
  };

  //

  class AdaptiveLocalTable : public HashTableStrategy
  {
  public:
    AdaptiveLocalTable(size_t capacity, double target_alpha,
                       size_t cluster_threshold,
                       double block_fill_limit = 0.85)
        : table_(capacity, kEmptyKey),
          capacity_(capacity),
          block_size_(practical_block_size(capacity, target_alpha)),
          block_loads_(num_blocks(capacity, block_size_), 0),
          cluster_threshold_(cluster_threshold),
          block_fill_limit_(block_fill_limit),
          rng_(0xBADB001ULL)
    {
      metrics_.auxiliary_memory_bytes = block_loads_.size() * sizeof(size_t);
    }

    bool insert(uint64_t key) override
    {
      if (key == kEmptyKey)
        return false;

      size_t h1 = static_cast<size_t>(hash1(key, capacity_));
      size_t local_cluster = measure_forward_cluster(h1);
      size_t bid = block_index(h1, block_size_);
      size_t cells = cells_in_block(bid, block_size_, capacity_);
      double bf = cells == 0
                      ? 1.0
                      : static_cast<double>(block_loads_[bid]) / static_cast<double>(cells);

      if (local_cluster < cluster_threshold_ && bf < block_fill_limit_)
        return insert_linear(h1, key);
      return insert_two_way(key);
    }

    bool search(uint64_t key) const override
    {
      uint64_t probes = 0;
      size_t idx1 = static_cast<size_t>(hash1(key, capacity_));
      size_t idx2 = static_cast<size_t>(hash2(key, capacity_));
      bool a1 = true;
      bool a2 = (idx1 != idx2);

      while (a1 || a2)
      {
        if (a1)
        {
          ++probes;
          if (table_[idx1] == key)
          {
            const_cast<AdaptiveLocalTable *>(this)
                ->metrics_.search_success_stats.record(probes);
            return true;
          }
          if (table_[idx1] == kEmptyKey)
            a1 = false;
          else
            idx1 = (idx1 + 1) % capacity_;
        }
        if (a2)
        {
          ++probes;
          if (table_[idx2] == key)
          {
            const_cast<AdaptiveLocalTable *>(this)
                ->metrics_.search_success_stats.record(probes);
            return true;
          }
          if (table_[idx2] == kEmptyKey)
            a2 = false;
          else
            idx2 = (idx2 + 1) % capacity_;
        }
      }
      const_cast<AdaptiveLocalTable *>(this)
          ->metrics_.search_fail_stats.record(probes);
      return false;
    }

    void clear() override
    {
      std::fill(table_.begin(), table_.end(), kEmptyKey);
      std::fill(block_loads_.begin(), block_loads_.end(), 0);
      size_ = 0;
      metrics_.reset_runtime();
      metrics_.max_cluster = 0;
    }

    size_t size() const override { return size_; }
    size_t capacity() const override { return capacity_; }
    double load_factor() const override
    {
      return capacity_ == 0 ? 0.0 : static_cast<double>(size_) / static_cast<double>(capacity_);
    }

    void reset_metrics() override { metrics_.reset_runtime(); }
    const TableMetrics &metrics() const override { return metrics_; }
    void refresh_cluster_metric() override
    {
      metrics_.max_cluster = compute_max_cluster(table_.data(), capacity_, kEmptyKey);
    }

    std::string name() const override { return "CAAP"; }
    std::string description() const override
    {
      return "CAAP v2: LP classico em baixa ocupacao local; two-way ao detectar cluster (busca dual-path).";
    }

  private:
    size_t measure_forward_cluster(size_t start) const
    {
      size_t count = 0, idx = start;
      while (table_[idx] != kEmptyKey)
      {
        ++count;
        idx = (idx + 1) % capacity_;
        if (idx == start)
          break;
      }
      return count;
    }

    bool insert_linear(size_t start, uint64_t key)
    {
      uint64_t probes = 0;
      size_t idx = start;
      while (true)
      {
        ++probes;
        if (table_[idx] == kEmptyKey)
        {
          table_[idx] = key;
          ++size_;
          ++block_loads_[block_index(idx, block_size_)];
          metrics_.insert_stats.record(probes);
          return true;
        }
        if (table_[idx] == key)
        {
          metrics_.insert_stats.record(probes);
          return false;
        }
        idx = (idx + 1) % capacity_;
      }
    }

    bool insert_two_way(uint64_t key)
    {
      size_t h1 = static_cast<size_t>(hash1(key, capacity_));
      size_t h2 = static_cast<size_t>(hash2(key, capacity_));
      uint64_t p1 = 0, p2 = 0;
      size_t u = terminal_empty(h1, p1);
      size_t v = terminal_empty(h2, p2);
      size_t bu = block_index(u, block_size_);
      size_t bv = block_index(v, block_size_);
      size_t chosen = u;
      if (block_loads_[bu] > block_loads_[bv])
        chosen = v;
      else if (block_loads_[bu] == block_loads_[bv] && u != v && coin_flip(rng_))
        chosen = v;
      table_[chosen] = key;
      ++size_;
      ++block_loads_[block_index(chosen, block_size_)];
      metrics_.insert_stats.record(p1 + p2);
      return true;
    }

    size_t terminal_empty(size_t start, uint64_t &probes) const
    {
      size_t idx = start;
      while (true)
      {
        ++probes;
        if (table_[idx] == kEmptyKey)
          return idx;
        idx = (idx + 1) % capacity_;
      }
    }

    std::vector<uint64_t> table_;
    size_t capacity_, block_size_;
    std::vector<size_t> block_loads_;
    size_t cluster_threshold_;
    double block_fill_limit_;
    size_t size_ = 0;
    mutable TableMetrics metrics_;
    mutable std::mt19937_64 rng_;
  };

  inline HashTablePtr make_linear_probing(size_t capacity)
  {
    return HashTablePtr(new LinearProbingTable(capacity));
  }

  inline HashTablePtr make_locally_linear(size_t capacity, double target_alpha)
  {
    return HashTablePtr(new LocallyLinearTable(capacity, target_alpha));
  }

  inline HashTablePtr make_walkfirst(size_t capacity, double target_alpha)
  {
    return HashTablePtr(new WalkFirstTable(capacity, target_alpha));
  }

  inline HashTablePtr make_adaptive_local(size_t capacity, double target_alpha,
                                          size_t cluster_threshold,
                                          double block_fill_limit)
  {
    return HashTablePtr(new AdaptiveLocalTable(capacity, target_alpha,
                                               cluster_threshold, block_fill_limit));
  }

  inline std::vector<HashTablePtr> all_strategies(size_t capacity, double target_alpha)
  {
    std::vector<HashTablePtr> s;
    s.push_back(make_linear_probing(capacity));
    s.push_back(make_locally_linear(capacity, target_alpha));
    s.push_back(make_walkfirst(capacity, target_alpha));
    s.push_back(make_adaptive_local(capacity, target_alpha, 8, 0.85));
    return s;
  }

}