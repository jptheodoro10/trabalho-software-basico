#include "cria_func.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if (!defined(__x86_64__) || !defined(__linux__)) && \
    !defined(CRIA_FUNC_TEST_FORCE_COMPILE)
int main(void)
{
  fprintf(stderr, "Este teste precisa ser executado em Linux x86-64.\n");
  return 77;
}
#else

#define CODE_LIMIT 256
#define GUARD_SIZE 64
#define RANDOM_CASES 5000

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

typedef int (*F0)(void);
typedef int (*F1I)(int);
typedef int (*F1P)(void *);
typedef int (*F2II)(int, int);
typedef int (*F2IP)(int, void *);
typedef int (*F2PI)(void *, int);
typedef int (*F2PP)(void *, void *);
typedef int (*F3III)(int, int, int);
typedef int (*F3IIP)(int, int, void *);
typedef int (*F3IPI)(int, void *, int);
typedef int (*F3IPP)(int, void *, void *);
typedef int (*F3PII)(void *, int, int);
typedef int (*F3PIP)(void *, int, void *);
typedef int (*F3PPI)(void *, void *, int);
typedef int (*F3PPP)(void *, void *, void *);

static uintptr_t received[3];
static int received_count;
static int target_return;
static unsigned long total_calls;
static unsigned long total_cases;
static unsigned long failures;

static int global_a;
static int global_b;

static uintptr_t int_bits(int value)
{
  return (uintptr_t)(uint32_t)value;
}

/*
 * Todas as funcoes-alvo chamam esta auxiliar. Alem de registrar os argumentos,
 * isso exercita uma chamada C normal feita pela funcao original.
 */
__attribute__((noinline))
static int record_values(uintptr_t a, uintptr_t b, uintptr_t c, int count)
{
  received[0] = a;
  received[1] = b;
  received[2] = c;
  received_count = count;
  total_calls++;
  return target_return;
}

static int target_i(int a)
{
  return record_values(int_bits(a), 0, 0, 1);
}

static int target_p(void *a)
{
  return record_values((uintptr_t)a, 0, 0, 1);
}

static int target_ii(int a, int b)
{
  return record_values(int_bits(a), int_bits(b), 0, 2);
}

static int target_ip(int a, void *b)
{
  return record_values(int_bits(a), (uintptr_t)b, 0, 2);
}

static int target_pi(void *a, int b)
{
  return record_values((uintptr_t)a, int_bits(b), 0, 2);
}

static int target_pp(void *a, void *b)
{
  return record_values((uintptr_t)a, (uintptr_t)b, 0, 2);
}

static int target_iii(int a, int b, int c)
{
  return record_values(int_bits(a), int_bits(b), int_bits(c), 3);
}

static int target_iip(int a, int b, void *c)
{
  return record_values(int_bits(a), int_bits(b), (uintptr_t)c, 3);
}

static int target_ipi(int a, void *b, int c)
{
  return record_values(int_bits(a), (uintptr_t)b, int_bits(c), 3);
}

static int target_ipp(int a, void *b, void *c)
{
  return record_values(int_bits(a), (uintptr_t)b, (uintptr_t)c, 3);
}

static int target_pii(void *a, int b, int c)
{
  return record_values((uintptr_t)a, int_bits(b), int_bits(c), 3);
}

static int target_pip(void *a, int b, void *c)
{
  return record_values((uintptr_t)a, int_bits(b), (uintptr_t)c, 3);
}

static int target_ppi(void *a, void *b, int c)
{
  return record_values((uintptr_t)a, (uintptr_t)b, int_bits(c), 3);
}

static int target_ppp(void *a, void *b, void *c)
{
  return record_values((uintptr_t)a, (uintptr_t)b, (uintptr_t)c, 3);
}

static void *select_target(int n, unsigned type_mask)
{
  if (n == 1)
    return type_mask == 0 ? (void *)target_i : (void *)target_p;

  if (n == 2)
  {
    void *targets[4] = {
      (void *)target_ii, (void *)target_pi,
      (void *)target_ip, (void *)target_pp
    };
    return targets[type_mask];
  }

  {
    void *targets[8] = {
      (void *)target_iii, (void *)target_pii,
      (void *)target_ipi, (void *)target_ppi,
      (void *)target_iip, (void *)target_pip,
      (void *)target_ipp, (void *)target_ppp
    };
    return targets[type_mask];
  }
}

static int call_generated(unsigned char *code, int param_count,
                          unsigned param_type_mask, uintptr_t args[3])
{
  if (param_count == 0)
    return ((F0)code)();

  if (param_count == 1)
  {
    if (param_type_mask == 0)
      return ((F1I)code)((int)(uint32_t)args[0]);
    return ((F1P)code)((void *)args[0]);
  }

  if (param_count == 2)
  {
    switch (param_type_mask)
    {
      case 0:
        return ((F2II)code)((int)(uint32_t)args[0],
                            (int)(uint32_t)args[1]);
      case 1:
        return ((F2PI)code)((void *)args[0],
                            (int)(uint32_t)args[1]);
      case 2:
        return ((F2IP)code)((int)(uint32_t)args[0],
                            (void *)args[1]);
      default:
        return ((F2PP)code)((void *)args[0], (void *)args[1]);
    }
  }

  switch (param_type_mask)
  {
    case 0:
      return ((F3III)code)((int)(uint32_t)args[0],
                           (int)(uint32_t)args[1],
                           (int)(uint32_t)args[2]);
    case 1:
      return ((F3PII)code)((void *)args[0],
                           (int)(uint32_t)args[1],
                           (int)(uint32_t)args[2]);
    case 2:
      return ((F3IPI)code)((int)(uint32_t)args[0],
                           (void *)args[1],
                           (int)(uint32_t)args[2]);
    case 3:
      return ((F3PPI)code)((void *)args[0], (void *)args[1],
                           (int)(uint32_t)args[2]);
    case 4:
      return ((F3IIP)code)((int)(uint32_t)args[0],
                           (int)(uint32_t)args[1],
                           (void *)args[2]);
    case 5:
      return ((F3PIP)code)((void *)args[0],
                           (int)(uint32_t)args[1],
                           (void *)args[2]);
    case 6:
      return ((F3IPP)code)((int)(uint32_t)args[0],
                           (void *)args[1], (void *)args[2]);
    default:
      return ((F3PPP)code)((void *)args[0], (void *)args[1],
                           (void *)args[2]);
  }
}

static const char *origin_name(OrigemValor origin)
{
  static const char *names[] = {"PARAM", "FIX", "IND"};
  return names[origin];
}

static void print_case(int n, DescParam params[3], int round,
                       const char *reason)
{
  int i;

  fprintf(stderr, "FALHA: n=%d rodada=%d [", n, round);
  for (i = 0; i < n; i++)
  {
    fprintf(stderr, "%s%s/%s", i == 0 ? "" : ",",
            origin_name(params[i].orig_val),
            params[i].tipo_val == INT_PAR ? "INT" : "PTR");
  }
  fprintf(stderr, "]: %s\n", reason);
}

static int guard_is_intact(unsigned char *code)
{
  int i;

  for (i = CODE_LIMIT; i < CODE_LIMIT + GUARD_SIZE; i++)
    if (code[i] != 0xa5)
      return 0;
  return 1;
}

static unsigned char *allocate_code(size_t *mapping_size)
{
  long page_size = sysconf(_SC_PAGESIZE);
  unsigned char *code;

  if (page_size < CODE_LIMIT + GUARD_SIZE)
    page_size = CODE_LIMIT + GUARD_SIZE;

  *mapping_size = (size_t)page_size;
  code = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE | PROT_EXEC,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (code == MAP_FAILED)
  {
    perror("mmap");
    exit(2);
  }
  return code;
}

static unsigned next_random(void)
{
  static uint32_t state = UINT32_C(0x6d2b79f5);

  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

static void run_case(int n, unsigned origins_code, unsigned type_mask,
                     int round, unsigned char *code)
{
  static const int edge_ints[] = {
    0, 1, -1, 2, -2, INT_MAX, INT_MIN, 0x12345678, -123456789
  };
  int local_a = 11;
  int local_b = 22;
  int *heap_a = malloc(sizeof(*heap_a));
  int *heap_b = malloc(sizeof(*heap_b));
  void *pointer_values[7];
  DescParam params[3];
  uintptr_t fixed_values[3] = {0, 0, 0};
  uintptr_t ind_values[3] = {0, 0, 0};
  uintptr_t ind_storage[3] = {0, 0, 0};
  uintptr_t wrapper_args[3] = {0, 0, 0};
  uintptr_t expected[3] = {0, 0, 0};
  unsigned param_type_mask = 0;
  int param_count = 0;
  int expected_return;
  int actual_return;
  int i;

  if (heap_a == NULL || heap_b == NULL)
  {
    fprintf(stderr, "malloc falhou\n");
    exit(2);
  }

  *heap_a = 33;
  *heap_b = 44;
  pointer_values[0] = NULL;
  pointer_values[1] = &global_a;
  pointer_values[2] = &global_b;
  pointer_values[3] = &local_a;
  pointer_values[4] = &local_b;
  pointer_values[5] = heap_a;
  pointer_values[6] = heap_b;

  for (i = 0; i < n; i++)
  {
    OrigemValor origin = (OrigemValor)(origins_code % 3);
    TipoValor type = (type_mask & (1u << i)) ? PTR_PAR : INT_PAR;
    uintptr_t value;

    origins_code /= 3;
    params[i].orig_val = origin;
    params[i].tipo_val = type;

    if (type == INT_PAR)
      value = int_bits(edge_ints[(round + i * 3) %
                                 (int)(sizeof(edge_ints) /
                                       sizeof(edge_ints[0]))]);
    else
      value = (uintptr_t)pointer_values[(round + i * 2) % 7];

    if (origin == PARAM)
    {
      wrapper_args[param_count] = value;
      if (type == PTR_PAR)
        param_type_mask |= 1u << param_count;
      expected[i] = value;
      param_count++;
    }
    else if (origin == FIX)
    {
      fixed_values[i] = value;
      expected[i] = value;
      if (type == INT_PAR)
        params[i].valor.v_int = (int)(uint32_t)value;
      else
        params[i].valor.v_ptr = (void *)value;
    }
    else
    {
      ind_storage[i] = value;
      ind_values[i] = value;
      expected[i] = value;
      params[i].valor.v_ptr = &ind_storage[i];
    }
  }

  memset(code, 0xcc, CODE_LIMIT);
  memset(code + CODE_LIMIT, 0xa5, GUARD_SIZE);
  cria_func(select_target(n, type_mask), params, n, code);
  total_cases++;

  if (!guard_is_intact(code))
  {
    failures++;
    print_case(n, params, round, "escreveu depois de CODE_LIMIT");
    goto cleanup;
  }

  /*
   * Alterar DescParam depois da geracao testa se FIX ficou embutido no codigo.
   * O resultado ainda deve usar fixed_values, e nao estes novos valores.
   */
  for (i = 0; i < n; i++)
  {
    if (params[i].orig_val == FIX)
    {
      if (params[i].tipo_val == INT_PAR)
        params[i].valor.v_int ^= 0x55aa55aa;
      else
        params[i].valor.v_ptr = pointer_values[(round + i + 3) % 7];
    }
  }

  expected_return = edge_ints[(round * 2 + 1) %
                              (int)(sizeof(edge_ints) /
                                    sizeof(edge_ints[0]))];
  target_return = expected_return;
  received_count = -1;
  actual_return = call_generated(code, param_count, param_type_mask,
                                 wrapper_args);

  if (actual_return != expected_return || received_count != n ||
      memcmp(received, expected, (size_t)n * sizeof(expected[0])) != 0)
  {
    failures++;
    print_case(n, params, round,
               "argumentos iniciais ou retorno incorretos");
    goto cleanup;
  }

  /*
   * IND precisa reler a memoria. FIX e PARAM devem continuar inalterados.
   * Trocar tambem entre NULL, stack, global e heap exercita ponteiros de 64 bits.
   */
  for (i = 0; i < n; i++)
  {
    if (params[i].orig_val == IND)
    {
      if (params[i].tipo_val == INT_PAR)
        ind_storage[i] =
          int_bits(edge_ints[(round + i + 5) %
                             (int)(sizeof(edge_ints) /
                                   sizeof(edge_ints[0]))]);
      else
        ind_storage[i] =
          (uintptr_t)pointer_values[(round + i + 1) % 7];
      ind_values[i] = ind_storage[i];
      expected[i] = ind_values[i];
    }
    else if (params[i].orig_val == FIX)
      expected[i] = fixed_values[i];
  }

  target_return = edge_ints[(round + 7) %
                            (int)(sizeof(edge_ints) /
                                  sizeof(edge_ints[0]))];
  received_count = -1;
  actual_return = call_generated(code, param_count, param_type_mask,
                                 wrapper_args);

  if (actual_return != target_return || received_count != n ||
      memcmp(received, expected, (size_t)n * sizeof(expected[0])) != 0)
  {
    failures++;
    print_case(n, params, round,
               "IND nao foi relido ou FIX/PARAM mudou");
  }

cleanup:
  free(heap_a);
  free(heap_b);
}

static void exhaustive_tests(unsigned char *code)
{
  int n;

  for (n = 1; n <= 3; n++)
  {
    unsigned origin_count = n == 1 ? 3u : (n == 2 ? 9u : 27u);
    unsigned type_count = 1u << n;
    unsigned origins;
    unsigned types;
    int round;

    for (origins = 0; origins < origin_count; origins++)
      for (types = 0; types < type_count; types++)
        for (round = 0; round < 9; round++)
          run_case(n, origins, types, round, code);
  }
}

static void random_tests(unsigned char *code)
{
  int i;

  for (i = 0; i < RANDOM_CASES; i++)
  {
    int n = (int)(next_random() % 3) + 1;
    unsigned origin_count = n == 1 ? 3u : (n == 2 ? 9u : 27u);
    unsigned origins = next_random() % origin_count;
    unsigned types = next_random() % (1u << n);
    int round = (int)(next_random() % 9);

    run_case(n, origins, types, round, code);
  }
}

static void interleaved_wrappers_test(void)
{
  size_t size_a;
  size_t size_b;
  unsigned char *code_a = allocate_code(&size_a);
  unsigned char *code_b = allocate_code(&size_b);
  DescParam params_a[3];
  DescParam params_b[3];
  uintptr_t args_a[3] = {int_bits(111), int_bits(222), 0};
  uintptr_t args_b[3] = {int_bits(-1), 0, 0};
  uintptr_t expected_a[3] = {int_bits(111), int_bits(7), int_bits(222)};
  uintptr_t expected_b[3] = {int_bits(9), int_bits(-1), int_bits(INT_MIN)};
  int ok = 1;

  params_a[0] = (DescParam){INT_PAR, PARAM, {.v_int = 0}};
  params_a[1] = (DescParam){INT_PAR, FIX, {.v_int = 7}};
  params_a[2] = (DescParam){INT_PAR, PARAM, {.v_int = 0}};

  params_b[0] = (DescParam){INT_PAR, FIX, {.v_int = 9}};
  params_b[1] = (DescParam){INT_PAR, PARAM, {.v_int = 0}};
  params_b[2] = (DescParam){INT_PAR, FIX, {.v_int = INT_MIN}};

  cria_func((void *)target_iii, params_a, 3, code_a);
  cria_func((void *)target_iii, params_b, 3, code_b);

  target_return = 123;
  call_generated(code_a, 2, 0, args_a);
  ok &= received_count == 3 &&
        memcmp(received, expected_a, sizeof(expected_a)) == 0;

  target_return = -456;
  call_generated(code_b, 1, 0, args_b);
  ok &= received_count == 3 &&
        memcmp(received, expected_b, sizeof(expected_b)) == 0;

  target_return = INT_MAX;
  call_generated(code_a, 2, 0, args_a);
  ok &= received_count == 3 &&
        memcmp(received, expected_a, sizeof(expected_a)) == 0;

  total_cases += 3;
  if (!ok)
  {
    failures++;
    fprintf(stderr, "FALHA: wrappers em buffers diferentes interferiram\n");
  }

  munmap(code_a, size_a);
  munmap(code_b, size_b);
}

static void shared_indirection_test(unsigned char *code)
{
  DescParam int_params[3];
  DescParam ptr_params[3];
  uintptr_t no_args[3] = {0, 0, 0};
  uintptr_t same_args[3];
  uintptr_t expected[3];
  uintptr_t shared_int;
  void *shared_ptr;
  int i;

  shared_int = int_bits(INT_MIN);
  for (i = 0; i < 3; i++)
  {
    int_params[i].tipo_val = INT_PAR;
    int_params[i].orig_val = IND;
    int_params[i].valor.v_ptr = &shared_int;
    expected[i] = shared_int;
  }

  cria_func((void *)target_iii, int_params, 3, code);
  target_return = 10;
  call_generated(code, 0, 0, no_args);
  total_cases++;
  if (received_count != 3 ||
      memcmp(received, expected, sizeof(expected)) != 0)
  {
    failures++;
    fprintf(stderr, "FALHA: tres IND/INT para a mesma variavel\n");
  }

  shared_int = int_bits(INT_MAX);
  expected[0] = shared_int;
  expected[1] = shared_int;
  expected[2] = shared_int;
  call_generated(code, 0, 0, no_args);
  total_cases++;
  if (received_count != 3 ||
      memcmp(received, expected, sizeof(expected)) != 0)
  {
    failures++;
    fprintf(stderr, "FALHA: IND/INT compartilhado nao foi relido\n");
  }

  shared_ptr = &global_a;
  for (i = 0; i < 3; i++)
  {
    ptr_params[i].tipo_val = PTR_PAR;
    ptr_params[i].orig_val = IND;
    ptr_params[i].valor.v_ptr = &shared_ptr;
    expected[i] = (uintptr_t)shared_ptr;
  }

  cria_func((void *)target_ppp, ptr_params, 3, code);
  call_generated(code, 0, 0, no_args);
  total_cases++;
  if (received_count != 3 ||
      memcmp(received, expected, sizeof(expected)) != 0)
  {
    failures++;
    fprintf(stderr, "FALHA: tres IND/PTR para a mesma variavel\n");
  }

  shared_ptr = NULL;
  expected[0] = 0;
  expected[1] = 0;
  expected[2] = 0;
  call_generated(code, 0, 0, no_args);
  total_cases++;
  if (received_count != 3 ||
      memcmp(received, expected, sizeof(expected)) != 0)
  {
    failures++;
    fprintf(stderr, "FALHA: IND/PTR compartilhado nao foi relido\n");
  }

  for (i = 0; i < 3; i++)
  {
    ptr_params[i].tipo_val = PTR_PAR;
    ptr_params[i].orig_val = PARAM;
    same_args[i] = (uintptr_t)&global_b;
    expected[i] = same_args[i];
  }

  cria_func((void *)target_ppp, ptr_params, 3, code);
  call_generated(code, 3, 7, same_args);
  total_cases++;
  if (received_count != 3 ||
      memcmp(received, expected, sizeof(expected)) != 0)
  {
    failures++;
    fprintf(stderr, "FALHA: tres PARAM receberam o mesmo ponteiro\n");
  }

  for (i = 0; i < 3; i++)
  {
    ptr_params[i].tipo_val = PTR_PAR;
    ptr_params[i].orig_val = FIX;
    ptr_params[i].valor.v_ptr = &global_a;
    expected[i] = (uintptr_t)&global_a;
  }

  cria_func((void *)target_ppp, ptr_params, 3, code);
  call_generated(code, 0, 0, no_args);
  total_cases++;
  if (received_count != 3 ||
      memcmp(received, expected, sizeof(expected)) != 0)
  {
    failures++;
    fprintf(stderr, "FALHA: tres FIX contem o mesmo ponteiro\n");
  }
}

int main(void)
{
  size_t mapping_size;
  unsigned char *code = allocate_code(&mapping_size);

  exhaustive_tests(code);
  random_tests(code);
  interleaved_wrappers_test();
  shared_indirection_test(code);

  munmap(code, mapping_size);

  printf("Estruturas exaustivas: 258\n");
  printf("Rodadas exaustivas por estrutura: 9\n");
  printf("Casos aleatorios: %d\n", RANDOM_CASES);
  printf("Geracoes/intercalacoes executadas: %lu\n", total_cases);
  printf("Chamadas de funcoes geradas: %lu\n", total_calls);

  if (failures != 0)
  {
    printf("RESULTADO: %lu falha(s)\n", failures);
    return 1;
  }

  printf("RESULTADO: todos os testes passaram\n");
  return 0;
}

#endif
