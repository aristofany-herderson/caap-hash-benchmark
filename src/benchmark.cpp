#include "hash_table.hpp"
#include "strategies.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{

  struct BenchConfig
  {
    size_t table_size = 65536;
    std::vector<double> load_factors = {0.20, 0.40, 0.60, 0.70, 0.80, 0.90, 0.95};
    size_t repetitions = 50;
    uint64_t seed = 42;
    std::string output_dir = "results";
  };

  struct BenchRow
  {
    std::string strategy;
    double load_factor;
    size_t repetition;
    size_t table_size;
    size_t keys_inserted;
    double avg_insert_probes;
    double avg_search_success_probes;
    double avg_search_fail_probes;
    size_t max_cluster;
    uint64_t worst_insert_probes;
    uint64_t worst_search_probes;
    double insert_ms;
    double search_ms;
    size_t aux_memory_bytes;
  };

  BenchConfig parse_args(int argc, char **argv)
  {
    BenchConfig cfg;
    for (int i = 1; i < argc; ++i)
    {
      std::string arg = argv[i];
      if (arg == "--table-size" && i + 1 < argc)
      {
        cfg.table_size = static_cast<size_t>(std::stoul(argv[++i]));
      }
      else if (arg == "--repetitions" && i + 1 < argc)
      {
        cfg.repetitions = static_cast<size_t>(std::stoul(argv[++i]));
      }
      else if (arg == "--seed" && i + 1 < argc)
      {
        cfg.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
      }
      else if (arg == "--output" && i + 1 < argc)
      {
        cfg.output_dir = argv[++i];
      }
      else if (arg == "--quick")
      {
        cfg.table_size = 8192;
        cfg.repetitions = 5;
      }
    }
    return cfg;
  }

  std::vector<uint64_t> shuffled_keys(size_t count, std::mt19937_64 &rng)
  {
    std::vector<uint64_t> keys(count);
    for (size_t i = 0; i < count; ++i)
      keys[i] = i + 1;
    std::shuffle(keys.begin(), keys.end(), rng);
    return keys;
  }

  BenchRow run_single(hp::HashTableStrategy &table,
                      const std::vector<uint64_t> &insert_keys,
                      const std::vector<uint64_t> &search_present,
                      const std::vector<uint64_t> &search_absent,
                      double target_load,
                      size_t repetition,
                      size_t table_size)
  {
    table.clear();
    table.reset_metrics();

    const auto t0 = std::chrono::steady_clock::now();
    for (uint64_t key : insert_keys)
    {
      table.insert(key);
    }
    const auto t1 = std::chrono::steady_clock::now();

    for (uint64_t key : search_present)
    {
      table.search(key);
    }
    for (uint64_t key : search_absent)
    {
      table.search(key);
    }
    const auto t2 = std::chrono::steady_clock::now();

    table.refresh_cluster_metric();
    const hp::TableMetrics &m = table.metrics();

    const double insert_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double search_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    uint64_t worst_search = m.search_success_stats.worst_probes;
    if (m.search_fail_stats.worst_probes > worst_search)
    {
      worst_search = m.search_fail_stats.worst_probes;
    }

    BenchRow row;
    row.strategy = table.name();
    row.load_factor = target_load;
    row.repetition = repetition;
    row.table_size = table_size;
    row.keys_inserted = insert_keys.size();
    row.avg_insert_probes = m.insert_stats.average();
    row.avg_search_success_probes = m.search_success_stats.average();
    row.avg_search_fail_probes = m.search_fail_stats.average();
    row.max_cluster = m.max_cluster;
    row.worst_insert_probes = m.insert_stats.worst_probes;
    row.worst_search_probes = worst_search;
    row.insert_ms = insert_ms;
    row.search_ms = search_ms;
    row.aux_memory_bytes = m.auxiliary_memory_bytes;
    return row;
  }

  void write_csv(const std::string &path, const std::vector<BenchRow> &rows)
  {
    std::ofstream out(path.c_str());
    out << "strategy,load_factor,repetition,table_size,keys_inserted,"
        << "avg_insert_probes,avg_search_success_probes,avg_search_fail_probes,"
        << "max_cluster,worst_insert_probes,worst_search_probes,"
        << "insert_ms,search_ms,aux_memory_bytes\n";

    out << std::fixed << std::setprecision(6);
    for (const BenchRow &r : rows)
    {
      out << r.strategy << ','
          << r.load_factor << ','
          << r.repetition << ','
          << r.table_size << ','
          << r.keys_inserted << ','
          << r.avg_insert_probes << ','
          << r.avg_search_success_probes << ','
          << r.avg_search_fail_probes << ','
          << r.max_cluster << ','
          << r.worst_insert_probes << ','
          << r.worst_search_probes << ','
          << r.insert_ms << ','
          << r.search_ms << ','
          << r.aux_memory_bytes << '\n';
    }
  }

  void print_summary(const std::vector<BenchRow> &rows)
  {
    std::cout << "\nResumo (media por estrategia e fator de carga):\n";
    std::cout << std::setw(16) << "estrategia"
              << std::setw(8) << "alpha"
              << std::setw(14) << "sond_ins"
              << std::setw(14) << "cluster_max"
              << std::setw(14) << "pior_ins"
              << std::setw(12) << "t_ins_ms"
              << '\n';

    for (double alpha : {0.20, 0.40, 0.60, 0.70, 0.80, 0.90, 0.95})
    {
      for (const char *name : {"LinearProbing", "LocallyLinear", "WalkFirst", "AdaptiveLocal"})
      {
        double sum_ins = 0, sum_cluster = 0, sum_worst = 0, sum_time = 0;
        size_t count = 0;
        for (const BenchRow &r : rows)
        {
          if (r.strategy == name && std::abs(r.load_factor - alpha) < 1e-9)
          {
            sum_ins += r.avg_insert_probes;
            sum_cluster += r.max_cluster;
            sum_worst += r.worst_insert_probes;
            sum_time += r.insert_ms;
            ++count;
          }
        }
        if (count == 0)
          continue;
        std::cout << std::setw(16) << name
                  << std::setw(8) << alpha
                  << std::setw(14) << (sum_ins / count)
                  << std::setw(14) << (sum_cluster / count)
                  << std::setw(14) << (sum_worst / count)
                  << std::setw(12) << (sum_time / count)
                  << '\n';
      }
    }
  }

} // namespace

int main(int argc, char **argv)
{
  BenchConfig cfg = parse_args(argc, argv);

  std::cout << "Hash Probing Study - IMD0029 UFRN\n";
  std::cout << "table_size=" << cfg.table_size
            << " repetitions=" << cfg.repetitions
            << " seed=" << cfg.seed << '\n';

#ifdef _WIN32
  std::string mkdir_cmd = "if not exist \"" + cfg.output_dir + "\" mkdir \"" + cfg.output_dir + "\"";
  std::system(mkdir_cmd.c_str());
#else
  std::system(("mkdir -p " + cfg.output_dir).c_str());
#endif

  std::vector<BenchRow> all_rows;
  std::mt19937_64 rng(cfg.seed);

  for (double alpha : cfg.load_factors)
  {
    size_t key_count = static_cast<size_t>(alpha * static_cast<double>(cfg.table_size));
    if (key_count == 0)
      continue;

    std::cout << "Fator de carga " << alpha << " (" << key_count << " chaves)...\n";

    for (size_t rep = 0; rep < cfg.repetitions; ++rep)
    {
      std::vector<uint64_t> keys = shuffled_keys(key_count, rng);
      std::vector<uint64_t> absent = shuffled_keys(key_count / 10 + 1, rng);
      for (uint64_t &k : absent)
        k += cfg.table_size + 1000;

      auto strategies = hp::all_strategies(cfg.table_size, alpha);
      for (auto &strategy : strategies)
      {
        BenchRow row = run_single(*strategy, keys, keys, absent, alpha, rep, cfg.table_size);
        all_rows.push_back(row);
      }
    }
  }

  const std::string csv_path = cfg.output_dir + "/benchmark_raw.csv";
  write_csv(csv_path, all_rows);
  std::cout << "CSV salvo em: " << csv_path << '\n';
  print_summary(all_rows);
  return 0;
}
