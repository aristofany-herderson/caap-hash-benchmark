#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace hp
{

  struct OperationStats
  {
    uint64_t total_probes = 0;
    uint64_t operations = 0;
    uint64_t worst_probes = 0;

    void record(uint64_t probes)
    {
      total_probes += probes;
      ++operations;
      if (probes > worst_probes)
        worst_probes = probes;
    }

    double average() const
    {
      if (operations == 0)
        return 0.0;
      return static_cast<double>(total_probes) / static_cast<double>(operations);
    }

    void reset()
    {
      total_probes = 0;
      operations = 0;
      worst_probes = 0;
    }
  };

  struct TableMetrics
  {
    OperationStats insert_stats;
    OperationStats search_success_stats;
    OperationStats search_fail_stats;
    size_t auxiliary_memory_bytes = 0;
    size_t max_cluster = 0;

    void reset_runtime()
    {
      insert_stats.reset();
      search_success_stats.reset();
      search_fail_stats.reset();
    }
  };

  inline size_t compute_max_cluster(const uint64_t *keys, size_t table_size, uint64_t empty_sentinel)
  {
    if (table_size == 0)
      return 0;

    size_t max_run = 0;
    size_t current = 0;
    bool any = false;

    for (size_t i = 0; i < table_size; ++i)
    {
      if (keys[i] != empty_sentinel)
      {
        ++current;
        any = true;
      }
      else
      {
        if (current > max_run)
          max_run = current;
        current = 0;
      }
    }
    if (current > max_run)
      max_run = current;

    if (!any)
      return 0;

    size_t wrap = 0;
    for (size_t i = 0; i < table_size && keys[i] != empty_sentinel; ++i)
      ++wrap;
    size_t tail = 0;
    for (size_t i = table_size; i > 0; --i)
    {
      if (keys[i - 1] != empty_sentinel)
        ++tail;
      else
        break;
    }
    if (wrap > 0 && tail > 0)
    {
      size_t merged = wrap + tail;
      if (merged > max_run)
        max_run = merged;
    }

    return max_run;
  }

}
