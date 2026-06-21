<p id="title" align="center">
  <a href="#title">
    <h1 align="center">CAAP - Cluster-Aware Adaptive Probing</h1>
  </a>
</p>

<p align="center">
  <a aria-label="Made By Aristofany" href="https://github.com/aristofany-herderson/">
    <img src="https://img.shields.io/badge/MADE%20BY%20Aristofany-000000.svg?style=for-the-badge&labelColor=000&logo=starship&logoColor=fff&logoWidth=20">
  </a>
  <a aria-label="Enjoy My Repos" href="https://github.com/aristofany-herderson?tab=repositories">
    <img alt="" src="https://img.shields.io/badge/Enjoy%20My%20Projects-000000.svg?style=for-the-badge&color=000&logo=github&labelColor=000000&logoColor=fff&logoWidth=20">
  </a>
</p>

<p align="center">💥 Análise experimental de estratégias de endereçamento aberto para tabelas hash, com uma suíte de benchmark em duas fases e varredura paramétrica automatizada</p>

<br>

## 🧪&nbsp; Tecnologias

Este projeto foi desenvolvido com as seguintes tecnologias e ferramentas:

- **C++17** - implementação das estratégias de hashing, compilada com `g++ -O2`.
- **Python 3** - geração de gráficos com `matplotlib`, `seaborn`, `pandas` e `numpy`.

<br>

## 🧑🏻‍💻&nbsp; Como Começar

Clone o projeto e acesse a pasta do repositório:

```bash
$ git clone https://github.com/aristofany-herderson/caap-hash-benchmark

$ cd caap-hash-benchmark
```

Crie o diretório de saída e compile:

```bash
# Criar diretório de build
$ mkdir -p build

# Compilar (C++17, otimização -O2)
$ g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src/benchmark.cpp -o build/benchmark
```

Execute a suíte de benchmarks:

```bash
# Execução completa (todas as fases)
$ ./build/benchmark

# Modo rápido - tabela 8 192, 2 seeds, 4 fatores de carga
$ ./build/benchmark --quick

# Apenas a fase 1 (sem varredura paramétrica)
$ ./build/benchmark --no-sweep

# Diretório de saída personalizado
$ ./build/benchmark --output meus_resultados
```

Para gerar os gráficos plotados:

```bash
# Instalar dependências Python
$ pip install matplotlib seaborn pandas numpy

# Gerar todos os PNGs em results/plots/
$ python scripts/plot_results.py

# Caminhos customizados
$ python scripts/plot_results.py \
    --main   results/benchmark_main.csv   \
    --params results/benchmark_params.csv \
    --output results/plots
```

### Argumentos de Linha de Comando Suportados:

| Flag           | Padrão    | Descrição                                                                                                                |
| -------------- | --------- | ------------------------------------------------------------------------------------------------------------------------ |
| `--quick`      | -         | Modo rápido: tabela única de 8 192 células, 2 seeds (`42`, `123`) e 4 fatores de carga (`0.50`, `0.70`, `0.90`, `0.95`). |
| `--no-sweep`   | -         | Executa apenas a Fase 1 (main sweep); pula a varredura paramétrica da estratégia `CAAP`.                                 |
| `--output DIR` | `results` | Diretório para salvamento dos CSVs de saída.                                                                             |
| `--help`       | -         | Exibe resumo das opções.                                                                                                 |

> **Nota:** As flags `--table-size`, `--repetitions` e `--seed` presentes em versões anteriores foram removidas. O espaço de busca agora é definido estaticamente na `BenchConfig` para garantir reprodutibilidade total da grade experimental.

<br />

## 💻&nbsp; O Projeto

**Resumo:** O trabalho compara algoritmos de resolução de colisão por endereçamento aberto: a sondagem linear clássica (_Linear Probing_) e duas variantes do paradigma _Two-Way Linear Probing_ com particionamento em blocos, propostas por Dalal, Devroye e Malalla (2023). A partir dessas referências, propomos uma heurística adaptativa original - **CAAP** (_Cluster-Aware Adaptive Probing_) - que alterna entre sondagem linear e two-way conforme o nível de congestionamento observado localmente.

<br />

## 🔬&nbsp; Protocolo Experimental (Suíte Bifásica)

A execução do benchmark é dividida em duas fases independentes que geram CSVs distintos.

### Fase 1 - Main Sweep

Todas as 4 estratégias são avaliadas em um produto cartesiano completo:

| Dimensão                 | Valores                                                                                        |
| ------------------------ | ---------------------------------------------------------------------------------------------- |
| **Tamanhos de tabela**   | `4 096`, `16 384`, `65 536`, `262 144`, `1 048 576`                                            |
| **Fatores de carga (α)** | `0.20`, `0.30`, `0.40`, `0.50`, `0.60`, `0.70`, `0.80`, `0.85`, `0.90`, `0.95`, `0.97`, `0.98` |
| **Seeds PRNG**           | `42`, `123`, `456`, `789`, `2025`, `31415`, `65537`, `99999`, `777777`, `888888`               |
| **Total de trials**      | 4 estratégias × 5 tamanhos × 12 α × 10 seeds = **2 400 trials**                                |

Saída: `results/benchmark_main.csv`

Em cada trial são coletadas as seguintes colunas:

| Coluna CSV                  | Descrição                                        |
| --------------------------- | ------------------------------------------------ |
| `avg_insert_probes`         | Média de sondas por inserção                     |
| `avg_search_success_probes` | Média de sondas - busca com sucesso              |
| `avg_search_fail_probes`    | Média de sondas - busca malsucedida              |
| `max_cluster`               | Maior cluster contíguo após inserções            |
| `worst_insert_probes`       | Pior caso de sondas na inserção                  |
| `worst_search_probes`       | Pior caso de sondas na busca                     |
| `insert_ms`                 | Tempo total de inserção (ms)                     |
| `search_ms`                 | Tempo total de busca (ms)                        |
| `aux_memory_bytes`          | Memória auxiliar usada (bytes de `block_loads_`) |

### Fase 2 - Parameter Sweep (AdaptiveLocal)

Varredura em grade dos dois hiperparâmetros da estratégia CAAP, fixando `table_size = 65 536`:

| Hiperparâmetro       | Valores varridos                                 |
| -------------------- | ------------------------------------------------ |
| `cluster_threshold`  | `2`, `4`, `6`, `8`, `10`, `12`, `16`, `20`, `24` |
| `block_fill_limit`   | `0.70`, `0.75`, `0.80`, `0.85`, `0.90`, `0.95`   |
| **Fatores de carga** | `0.60`, `0.70`, `0.80`, `0.90`, `0.95`, `0.97`   |
| **Seeds**            | mesmas 10 da Fase 1                              |
| **Total de trials**  | 9 × 6 × 6 × 10 = **3 240 trials**                |

Saída: `results/benchmark_params.csv`. Esta fase pode ser ignorada com `--no-sweep`.

<br />

## 📐&nbsp; Arquitetura das Estratégias Analisadas

Abaixo está o diagrama macro estrutural de funcionamento:

<img width="100%" src="./hash-strategies.svg">

---

### `LinearProbingTable` - baseline

Quando inserimos uma chave k, calculamos `idx = hash1(k) % capacity` e percorremos linearmente até encontrar uma célula vazia. A busca faz o mesmo percurso: parte de `hash1(k)` e anda para a direita; se bate numa célula vazia, a chave não existe.

O problema clássico é o _primary clustering_: colisões em torno do mesmo `idx` formam um aglomerado contínuo, e qualquer nova chave que caia nessa vizinhança prolonga o aglomerado. O Teorema 1 de Dalal et al. (2023) prova que o maior cluster cresce como Ω(log n) com probabilidade alta.

---

### `LocallyLinearTable` - LOCALLYLINEAR (Dalal et al., 2023)

A tabela é particionada em blocos de tamanho β calculado pela fórmula derivada do Teorema 8:

```
β = ⌈ log₂(log₂ n) / (1 − α) ⌉
```

Cada bloco mantém um contador de quantas chaves ele contém (`block_loads_`). Na inserção, duas células iniciais são sorteadas (`h1` e `h2`) e identifica-se a qual bloco cada uma pertence. O algoritmo escolhe o bloco menos carregado dos dois. A partir da célula inicial daquele bloco, sonda linearmente dentro do bloco - ciclicamente se necessário. Se o bloco estiver lotado, avança para o bloco vizinho à direita.

A busca percorre os dois blocos iniciais (de `h1` e `h2`) e, se ambos estiverem cheios e a chave não foi encontrada, avança pelos blocos vizinhos em profundidade.

**Resultado teórico:** pior caso O(log log n) para busca malsucedida (Teorema 8).

---

### `WalkFirstTable` - WALKFIRST (Dalal et al., 2023)

A diferença para `LocallyLinear` está em _onde_ a sondagem linear acontece: não dentro do bloco, mas ao longo da tabela inteira. Dadas as células iniciais `h1` e `h2`, o algoritmo anda linearmente a partir de cada uma até encontrar dois terminais `u` e `v` - as primeiras células vazias que cada percurso encontra. Insere no terminal cujo bloco tiver menor carga entre `bloco(u)` e `bloco(v)`.

A busca percorre ambas as cadeias em _lock-step_, avançando `idx1` e `idx2` alternadamente até encontrar a chave ou ambas as cadeias terminarem em célula vazia (busca dual-path).

**Resultado teórico:** O(log log n) para α < 1/2 (Teorema 10); as simulações do paper sugerem validade para qualquer α constante.

---

### `AdaptiveLocalTable` - CAAP (heurística original)

**Inserção adaptativa:** antes de inserir, mede dois indicadores locais a partir de `h1(k)`: o tamanho do cluster à frente (`measure_forward_cluster`) e a taxa de ocupação do bloco correspondente (`block_fill`).

```
se cluster < cluster_threshold  AND  block_fill < block_fill_limit:
    → inserção Linear clássica (barata)
caso contrário:
    → inserção WalkFirst two-way (redirecionamento por menor bloco)
```

A lógica é: em regiões pouco congestionadas, o custo extra de calcular `h2` e explorar dois terminais não compensa; só quando há saturação local é que o redirecionamento ajuda.

<br />

## ⚙️&nbsp; Justificativa dos Parâmetros τ e φ

Os dois hiperparâmetros do CAAP foram calibrados por varredura empírica (Fase 2, 49 pares testados por fator de carga) e por uma análise teórica aproximada a partir do tamanho de cluster esperado sob sondagem linear, E[γ] = α / (1 − α).

**φ = 0.95** domina consistentemente os demais valores testados, em todos os fatores de carga: o cluster local γ(k) se mostra um preditor mais relevante de congestionamento do que a ocupação do bloco isoladamente.

**τ** tem efeito não monotônico e depende de α: valores maiores (τ ≈ 10–20) são preferíveis para α ≤ 0.80, evitando overhead desnecessário do modo two-way; valores menores (τ ≈ 4–8) funcionam melhor para α ≥ 0.90, ativando o two-way antes que clusters grandes se formem.

**Padrão conservador**, recomendado quando α não é conhecido a priori: `τ = 8`, `φ = 0.85`.

`kEmptyKey = UINT64_MAX` é usado como sentinela, já que as chaves geradas no benchmark variam em `1..key_count`.

Seeds distintas (`0xC0FFEEULL`, `0xFACEFEEDULL`, `0xBADB001ULL`) são usadas para `LocallyLinear`, `WalkFirst` e CAAP respectivamente, evitando correlação entre desempates nas três tabelas dentro da mesma rodada.

As 10 seeds por configuração foram suficientes para estabilizar médias e intervalos de confiança de 95% nos experimentos reportados no artigo.

<br />

## 🔧&nbsp; Funções de Hash

Ambas as funções de hash derivam do `splitmix64`, garantindo distribuição de alta qualidade e ausência de correlação entre `hash1` e `hash2`:

```cpp
// Misturador base
uint64_t splitmix64(uint64_t x);

// hash primário
hash1(k) = splitmix64(k * 0xD6E8FEB86659FD93) % capacity

// hash secundário (multiplicador e offset distintos)
hash2(k) = splitmix64(k * 0xA5A356AA556A5B77 + 0x9E3779B97F4A7C15) % capacity
```

<br />

## 🟰&nbsp; Resultados principais

Resumo dos achados reportados no artigo (tabela completa em `results/`, n = 65 536 salvo indicação contrária):

- O CAAP reduz o número médio de sondas de inserção em até **44%** frente ao LinearProbing, e supera o WalkFirst em todos os fatores de carga testados.
- O maior cluster (`max_cluster`) é reduzido em até **93,7%** frente ao LinearProbing para α = 0.97, ficando estatisticamente equivalente ao WalkFirst.
- A busca bem-sucedida do CAAP é a mais barata entre todas as estratégias para α ≥ 0.70 (4.585 sondas para α = 0.90, contra 5.079 do WalkFirst e 5.613 do LinearProbing).
- O crescimento de `max_cluster` com o tamanho da tabela é compatível com O(log log n): para α = 0.90, ao variar n de 4 096 para 1 048 576 (fator de 256×), o cluster do CAAP cresce apenas 1,3×.
- A memória auxiliar do CAAP é idêntica à das demais estratégias two-way: `⌈n/β⌉` contadores de 64 bits — cerca de 20 KB (0,4% da tabela) para n = 65 536, α = 0.90.
  Limitações: o CAAP não supera o LocallyLinear no custo médio de inserção, dado que a sondagem inteiramente local deste último é mais previsível; a busca malsucedida do CAAP também fica acima da do LocallyLinear. O ajuste de τ e φ por faixa de α melhora os resultados, mas não há, até o momento, garantia formal de pior caso para a heurística — os resultados são empíricos.

<br>

## 📊&nbsp; Visualizações Geradas

O script `scripts/plot_results.py` produz quatro famílias de gráficos em `results/plots/`:

**Linha com IC-95 (`line_*.png`)** - para cada uma das 6 métricas, um painel com subplots por tamanho de tabela, com barras de erro de 1,96 × SEM (intervalo de confiança de 95% sobre as 10 seeds). Permite avaliar como cada estratégia escala com α.

**Box-plots (`boxplot_alpha*.png`)** - distribuição das métricas sobre as 10 seeds para α ∈ {0.70, 0.80, 0.90, 0.95, 0.97} com `n = 65 536`. Revela dispersão e presença de outliers por estratégia.

**Escalabilidade (`scalability.png`)** - eixo x em escala log₂ do tamanho de tabela, fixado α = 0.90. Avalia crescimento assintótico de cada estratégia.

**Heatmaps de varredura paramétrica (`heatmap_*.png`)** - grid `cluster_threshold × block_fill_limit` colorido pelo valor médio da métrica para α ∈ {0.80, 0.90, 0.95, 0.97}. A célula ótima é marcada com borda azul. Gerado apenas com dados da Fase 2.

Além dos PNGs, o script salva dois CSVs adicionais:

| Arquivo                         | Conteúdo                                                                         |
| ------------------------------- | -------------------------------------------------------------------------------- |
| `results/benchmark_summary.csv` | Médias e SEMs agregados por (estratégia, α, tamanho)                             |
| `results/best_params.csv`       | Melhor `(cluster_threshold, block_fill_limit)` por α segundo `avg_insert_probes` |

<br />

## 📁&nbsp; Estrutura do Projeto

```
caap-hash-benchmark/
├── include/
│   ├── hash_table.hpp       # Interface polimórfica HashTableStrategy + fábrica
│   ├── hash_utils.hpp       # splitmix64, hash1/2, utilitários de bloco
│   ├── metrics.hpp          # OperationStats, TableMetrics, compute_max_cluster
│   └── strategies.hpp       # LinearProbing, LocallyLinear, WalkFirst, AdaptiveLocal
├── src/
│   └── benchmark.cpp        # BenchConfig, Fase 1, Fase 2, CSVs, main
├── scripts/
│   └── plot_results.py      # Linha IC-95, box-plots, escalabilidade, heatmaps
├── results/                 # (gerado em tempo de execução)
│   ├── benchmark_main.csv
│   ├── benchmark_params.csv
│   ├── benchmark_summary.csv
│   ├── best_params.csv
│   └── plots/
└── hash-strategies.svg
```

<br />

## 📚&nbsp; Referência

DALAL, K.; DEVROYE, L.; MALALLA, E. **Two-Way Linear Probing Revisited**. _Algorithms_, v. 16, n. 11, p. 500, 2023. https://doi.org/10.3390/a16110500

KNUTH, D. **The Art of Computer Programming, Vol. 3: Sorting and Searching**. Addison-Wesley, 1998.

CORMEN, T. et al. **Algoritmos: teoria e prática**. 3. ed. Elsevier, 2012.

PAGH, R.; RODLER, F. **Cuckoo Hashing**. _Journal of Algorithms_, v. 51, n. 2, p. 122–144, 2003.

FARACH-COLTON, M.; KRAPIVIN, A.; KUSZMAUL, W. **Optimal Bounds for Open Addressing Without Reordering**. arXiv:2402.04339, 2024.

MELIS, G. **Adaptive Hashing: Faster Hash Functions with Fewer Collisions**. arXiv:2602.05925, 2026.

WANG, Q. **The Bathroom Model: A Realistic Approach to Hash Table Algorithm Optimization**. arXiv:2502.10977, 2025.

<br />

## 🧑🏻&nbsp; Autor

<p align="center">
  <img width="20%" src="https://github.com/aristofany-herderson.png" alt="aristofany-herderson">
  <p align="center">
    Aristofany Herderson
  </p>
  <p align="center">
    <a href="https://www.linkedin.com/in/aristofany-herderson/" target="_blank">
      <img align="center" src="https://img.shields.io/badge/LINKEDIN-000000.svg?style=for-the-badge&labelColor=0a66c2&logo=inspire&logoColor=fff&logoWidth=20" alt="linkedin"/>
    </a>
    <a href="https://www.instagram.com/aristo.dev/" target="_blank">
      <img align="center" src="https://img.shields.io/badge/INSTAGRAM-000000.svg?style=for-the-badge&labelColor=dd326f&logo=instagram&logoColor=fff&logoWidth=20" alt="instagram"/>
    </a>
  </p>
</p>
