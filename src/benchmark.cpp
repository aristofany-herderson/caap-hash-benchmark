

#include "hash_table.hpp"
#include "strategies.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace
{

  struct BenchConfig
  {

    std::vector<size_t> table_sizes = {4096, 16384, 65536, 262144, 1048576};
    std::vector<double> load_factors = {
        0.20, 0.30, 0.40, 0.50, 0.60,
        0.70, 0.80, 0.85, 0.90, 0.95, 0.97, 0.98};
    std::vector<uint64_t> seeds = {
        42, 123, 456, 789, 2025,
        31415, 65537, 99999, 777777, 888888};

    std::vector<size_t> ct_values = {2, 4, 6, 8, 10, 12, 16, 20, 24};
    std::vector<double> bf_values = {0.70, 0.75, 0.80, 0.85, 0.90, 0.95};
    size_t sweep_tsize = 65536;
    std::vector<double> sweep_alphas = {0.60, 0.70, 0.80, 0.90, 0.95, 0.97};

    std::string output_dir = "results";
    bool quick = false;
    bool no_sweep = false;
  };

  struct MainRow
  {
    std::string strategy;
    double load_factor;
    size_t table_size;
    uint64_t seed;
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

  struct ParamRow
  {
    size_t cluster_threshold;
    double block_fill_limit;
    double load_factor;
    size_t table_size;
    uint64_t seed;
    size_t keys_inserted;
    double avg_insert_probes;
    double avg_search_success_probes;
    double avg_search_fail_probes;
    size_t max_cluster;
    uint64_t worst_insert_probes;
    double insert_ms;
    size_t aux_memory_bytes;
  };

  BenchConfig parse_args(int argc, char **argv)
  {
    BenchConfig cfg;
    for (int i = 1; i < argc; ++i)
    {
      std::string a = argv[i];
      if (a == "--quick")
      {
        cfg.quick = true;
        cfg.table_sizes = {8192};
        cfg.seeds = {42, 123};
        cfg.load_factors = {0.50, 0.70, 0.90, 0.95};
        cfg.sweep_tsize = 8192;
        cfg.sweep_alphas = {0.70, 0.90, 0.95};
      }
      else if (a == "--no-sweep")
      {
        cfg.no_sweep = true;
      }
      else if (a == "--output" && i + 1 < argc)
      {
        cfg.output_dir = argv[++i];
      }
      else if (a == "--help")
      {
        std::cout << "Usage: benchmark [options]\n"
                     "  --quick          Fast mode (small table, 2 seeds)\n"
                     "  --no-sweep       Skip parameter sweep\n"
                     "  --output DIR     Output directory (default: results)\n";
        std::exit(0);
      }
    }
    return cfg;
  }

  std::vector<uint64_t> shuffled_keys(size_t count, std::mt19937_64 &rng)
  {
    std::vector<uint64_t> keys(count);
    std::iota(keys.begin(), keys.end(), uint64_t{1});
    std::shuffle(keys.begin(), keys.end(), rng);
    return keys;
  }

  MainRow run_main_trial(hp::HashTableStrategy &table,
                         const std::vector<uint64_t> &insert_keys,
                         const std::vector<uint64_t> &absent_keys,
                         double alpha, size_t tsize, uint64_t seed)
  {
    table.clear();
    table.reset_metrics();

    auto t0 = std::chrono::steady_clock::now();
    for (uint64_t k : insert_keys)
      table.insert(k);
    auto t1 = std::chrono::steady_clock::now();

    for (uint64_t k : insert_keys)
      table.search(k);

    for (uint64_t k : absent_keys)
      table.search(k);
    auto t2 = std::chrono::steady_clock::now();

    table.refresh_cluster_metric();
    const hp::TableMetrics &m = table.metrics();

    double ins_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double srch_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    uint64_t worst_s = std::max(m.search_success_stats.worst_probes,
                                m.search_fail_stats.worst_probes);

    return {
        table.name(), alpha, tsize, seed,
        insert_keys.size(),
        m.insert_stats.average(),
        m.search_success_stats.average(),
        m.search_fail_stats.average(),
        m.max_cluster,
        m.insert_stats.worst_probes,
        worst_s,
        ins_ms, srch_ms,
        m.auxiliary_memory_bytes};
  }

  void write_main_csv(const std::string &path, const std::vector<MainRow> &rows)
  {
    std::ofstream out(path);
    if (!out)
    {
      std::cerr << "Cannot open " << path << '\n';
      return;
    }
    out << "strategy,load_factor,table_size,seed,"
           "keys_inserted,avg_insert_probes,avg_search_success_probes,"
           "avg_search_fail_probes,max_cluster,worst_insert_probes,"
           "worst_search_probes,insert_ms,search_ms,aux_memory_bytes\n";
    out << std::fixed << std::setprecision(6);
    for (const auto &r : rows)
      out << r.strategy << ','
          << r.load_factor << ','
          << r.table_size << ','
          << r.seed << ','
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

  void write_param_csv(const std::string &path, const std::vector<ParamRow> &rows)
  {
    std::ofstream out(path);
    if (!out)
    {
      std::cerr << "Cannot open " << path << '\n';
      return;
    }
    out << "cluster_threshold,block_fill_limit,load_factor,table_size,seed,"
           "keys_inserted,avg_insert_probes,avg_search_success_probes,"
           "avg_search_fail_probes,max_cluster,worst_insert_probes,"
           "insert_ms,aux_memory_bytes\n";
    out << std::fixed << std::setprecision(6);
    for (const auto &r : rows)
      out << r.cluster_threshold << ','
          << r.block_fill_limit << ','
          << r.load_factor << ','
          << r.table_size << ','
          << r.seed << ','
          << r.keys_inserted << ','
          << r.avg_insert_probes << ','
          << r.avg_search_success_probes << ','
          << r.avg_search_fail_probes << ','
          << r.max_cluster << ','
          << r.worst_insert_probes << ','
          << r.insert_ms << ','
          << r.aux_memory_bytes << '\n';
  }

  void progress(size_t done, size_t total, const std::string &tag)
  {
    int pct = static_cast<int>(100.0 * done / total);
    constexpr int W = 30;
    int fill = W * pct / 100;
    std::cout << '\r' << tag << " [";
    for (int i = 0; i < W; ++i)
      std::cout << (i < fill ? '#' : ' ');
    std::cout << "] " << std::setw(3) << pct << "% ("
              << done << '/' << total << ')' << std::flush;
    if (done == total)
      std::cout << '\n';
  }

}

int main(int argc, char **argv)
{
  BenchConfig cfg = parse_args(argc, argv);

  std::cout << "###########################################################\n"
            << "#  CAAP Benchmark Suite - Cluster-Aware Adaptive Probing  #\n"
            << "###########################################################\n";
  if (cfg.quick)
    std::cout << "[quick mode]\n";

#ifdef _WIN32
  std::system(("if not exist \"" + cfg.output_dir + "\" mkdir \"" + cfg.output_dir + "\"").c_str());
#else
  std::system(("mkdir -p " + cfg.output_dir).c_str());
#endif

  std::vector<MainRow> main_rows;
  {
    const size_t total =
        4 * cfg.table_sizes.size() * cfg.load_factors.size() * cfg.seeds.size();
    size_t done = 0;

    std::cout << "\n[1/2] Main sweep - " << total << " trials\n";

    for (size_t tsize : cfg.table_sizes)
    {
      for (double alpha : cfg.load_factors)
      {
        size_t n_keys = static_cast<size_t>(alpha * static_cast<double>(tsize));
        if (n_keys == 0)
          continue;

        for (uint64_t seed : cfg.seeds)
        {
          std::mt19937_64 rng(seed);
          auto ins_keys = shuffled_keys(n_keys, rng);

          size_t n_absent = n_keys / 10 + 1;
          auto abs_keys = shuffled_keys(n_absent, rng);
          for (auto &k : abs_keys)
            k += tsize + 100000ULL;

          auto strategies = hp::all_strategies(tsize, alpha);
          for (auto &s : strategies)
          {
            main_rows.push_back(
                run_main_trial(*s, ins_keys, abs_keys, alpha, tsize, seed));
            progress(++done, total, "  Phase 1");
          }
        }
      }
    }
  }

  const std::string main_csv = cfg.output_dir + "/benchmark_main.csv";
  write_main_csv(main_csv, main_rows);
  std::cout << "  Saved: " << main_csv << "  (" << main_rows.size() << " rows)\n";

  if (!cfg.no_sweep)
  {
    std::vector<ParamRow> param_rows;
    const size_t total =
        cfg.ct_values.size() * cfg.bf_values.size() * cfg.sweep_alphas.size() * cfg.seeds.size();
    size_t done = 0;

    std::cout << "\n[2/2] Parameter sweep - " << total
              << " trials  (table_size=" << cfg.sweep_tsize << ")\n";

    for (size_t ct : cfg.ct_values)
    {
      for (double bf : cfg.bf_values)
      {
        for (double alpha : cfg.sweep_alphas)
        {
          size_t n_keys = static_cast<size_t>(
              alpha * static_cast<double>(cfg.sweep_tsize));
          if (n_keys == 0)
            continue;

          for (uint64_t seed : cfg.seeds)
          {
            std::mt19937_64 rng(seed);
            auto ins_keys = shuffled_keys(n_keys, rng);
            size_t n_absent = n_keys / 10 + 1;
            auto abs_keys = shuffled_keys(n_absent, rng);
            for (auto &k : abs_keys)
              k += cfg.sweep_tsize + 100000ULL;

            auto table = hp::make_adaptive_local(
                cfg.sweep_tsize, alpha, ct, bf);
            table->clear();
            table->reset_metrics();

            auto t0 = std::chrono::steady_clock::now();
            for (uint64_t k : ins_keys)
              table->insert(k);
            auto t1 = std::chrono::steady_clock::now();
            for (uint64_t k : ins_keys)
              table->search(k);
            for (uint64_t k : abs_keys)
              table->search(k);

            table->refresh_cluster_metric();
            const hp::TableMetrics &m = table->metrics();
            double ins_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            param_rows.push_back({ct, bf, alpha, cfg.sweep_tsize, seed,
                                  n_keys,
                                  m.insert_stats.average(),
                                  m.search_success_stats.average(),
                                  m.search_fail_stats.average(),
                                  m.max_cluster,
                                  m.insert_stats.worst_probes,
                                  ins_ms,
                                  m.auxiliary_memory_bytes});
            progress(++done, total, "  Phase 2");
          }
        }
      }
    }

    const std::string param_csv = cfg.output_dir + "/benchmark_params.csv";
    write_param_csv(param_csv, param_rows);
    std::cout << "  Saved: " << param_csv << "  (" << param_rows.size() << " rows)\n";
  }

  std::cout << "\nAll done.  Run  python scripts/plot_results.py  to generate plots.\n";
  return 0;
}