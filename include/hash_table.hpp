#pragma once

#include "metrics.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hp
{

  class HashTableStrategy
  {
  public:
    virtual ~HashTableStrategy() = default;

    virtual bool insert(uint64_t key) = 0;
    virtual bool search(uint64_t key) const = 0;

    virtual void clear() = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual double load_factor() const = 0;

    virtual void reset_metrics() = 0;
    virtual const TableMetrics &metrics() const = 0;
    virtual void refresh_cluster_metric() = 0;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
  };

  using HashTablePtr = std::unique_ptr<HashTableStrategy>;

  HashTablePtr make_linear_probing(size_t capacity);
  HashTablePtr make_locally_linear(size_t capacity, double target_alpha);
  HashTablePtr make_walkfirst(size_t capacity, double target_alpha);

  HashTablePtr make_adaptive_local(size_t capacity, double target_alpha,
                                   size_t cluster_threshold,
                                   double block_fill_limit = 0.85);

  std::vector<HashTablePtr> all_strategies(size_t capacity, double target_alpha);

}