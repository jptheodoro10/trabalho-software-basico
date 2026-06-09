# Gerador Dinamico de Funcoes

Trabalho de INF1018 para gerar, em tempo de execucao, uma nova funcao que chama uma funcao C original com alguns parametros repassados, fixados ou lidos indiretamente da memoria.

O arquivo principal entregue e `cria_func.c`. Ele implementa:

```c
void cria_func(void *f, DescParam params[], int n, unsigned char codigo[]);
```

`cria_func` grava codigo de maquina x86-64 dentro do vetor `codigo`. Depois, o programa de teste converte esse vetor para ponteiro de funcao e chama a funcao gerada.

## Arquivos

- `cria_func.h`: definicoes de `DescParam`, `TipoValor`, `OrigemValor` e prototipo de `cria_func`.
- `cria_func.c`: implementacao do gerador de codigo de maquina.
- `teste.c`: exemplo simples de teste.

## Requisitos

Teste em Linux x86-64, que e o ambiente esperado pelo trabalho.

Este codigo gera instrucoes para a ABI System V x86-64, usando os registradores:

```text
parametro 0 -> %rdi / %edi
parametro 1 -> %rsi / %esi
parametro 2 -> %rdx / %edx
```

No macOS ARM64, o codigo pode compilar como C, mas a funcao gerada nao deve ser executada, porque as instrucoes emitidas sao x86-64.

## Como Compilar

No Linux, dentro da pasta do repo:

```sh
gcc -Wall -Wa,--execstack -o teste cria_func.c teste.c
```

A opcao `-Wa,--execstack` permite executar o codigo gravado no vetor local `codigo`.

Se o sistema reclamar dessa opcao, tente:

```sh
gcc -Wall -z execstack -o teste cria_func.c teste.c
```

## Como Rodar

Depois de compilar:

```sh
./teste
```

Com o `teste.c` atual, a funcao original e:

```c
int mult(int x, int y)
{
  return x * y;
}
```

E os parametros estao configurados assim:

```c
params[0].orig_val = IND; /* le o valor atual de i */
params[1].orig_val = FIX; /* usa a constante 10 */
```

Ou seja, a funcao gerada nao recebe argumentos. Ela chama:

```c
mult(i, 10)
```

Por isso a saida esperada e:

```text
10
20
30
40
50
60
70
80
90
100
```

## O Que Cada Origem Faz

Cada posicao de `params` descreve um parametro da funcao original.

### PARAM

O valor vem de um argumento recebido pela funcao gerada.

Exemplo:

```c
params[0].orig_val = PARAM;
params[1].orig_val = FIX;
```

A funcao original tem 2 parametros, mas a funcao gerada recebe apenas 1. Esse argumento recebido pela funcao gerada e repassado para o parametro 0 da funcao original.

### FIX

O valor e uma constante guardada em `params[i].valor`.

Para inteiro:

```c
params[i].tipo_val = INT_PAR;
params[i].orig_val = FIX;
params[i].valor.v_int = 10;
```

Para ponteiro:

```c
params[i].tipo_val = PTR_PAR;
params[i].orig_val = FIX;
params[i].valor.v_ptr = algum_ponteiro;
```

### IND

O valor e lido da memoria no momento da chamada.

Exemplo:

```c
int i;
params[0].tipo_val = INT_PAR;
params[0].orig_val = IND;
params[0].valor.v_ptr = &i;
```

Nesse caso, a funcao gerada nao fixa o valor antigo de `i`. Ela le o valor atual de `i` toda vez que for chamada.

## Por Que Existe `origem_param`

Dentro de `cria_func.c`, este trecho calcula quais parametros da funcao original realmente vem dos argumentos da funcao gerada:

```c
for (i = 0; i < n; i++)
{
  if (params[i].orig_val == PARAM)
    origem_param[i] = prox_param++;
  else
    origem_param[i] = -1;
}
```

`origem_param[i]` so importa quando `params[i].orig_val == PARAM`.

Para `FIX` e `IND`, ele fica `-1` porque esses valores nao vem dos argumentos recebidos pela funcao gerada:

- `FIX` vem de uma constante em `params[i].valor`;
- `IND` vem da memoria apontada por `params[i].valor.v_ptr`.

## Exemplo: FIX FIX PARAM

Considere:

```c
params[0].orig_val = FIX;
params[1].orig_val = FIX;
params[2].orig_val = PARAM;
```

A funcao original tem 3 parametros:

```c
original(arg0, arg1, arg2)
```

Mas a funcao gerada recebe apenas 1 argumento, porque somente um parametro e `PARAM`.

Na ABI x86-64, esse unico argumento chega em:

```text
%rdi / %edi
```

So que ele precisa ser passado como terceiro parametro da funcao original, que fica em:

```text
%rdx / %edx
```

Entao o codigo gerado precisa mover:

```asm
mov %edi, %edx
```

para `INT_PAR`, ou:

```asm
mov %rdi, %rdx
```

para `PTR_PAR`.

Por isso `origem_param` ficaria:

```c
origem_param[0] = -1;
origem_param[1] = -1;
origem_param[2] = 0;
```

O `0` em `origem_param[2]` significa: o parametro original de indice 2 vem do argumento 0 da funcao gerada.

## Por Que O Loop De Geracao Anda De Tras Para Frente

Depois de calcular `origem_param`, o codigo percorre `params` de tras para frente:

```c
for (i = n - 1; i >= 0; i--)
{
  ...
}
```

Isso evita perder valores quando um argumento precisa mudar de registrador.

Exemplo `FIX FIX PARAM`:

```text
entrada da funcao gerada:
argumento 0 em %edi

chamada da funcao original:
parametro 0 em %edi
parametro 1 em %esi
parametro 2 em %edx
```

Se o codigo carregasse primeiro o `FIX` do parametro 0 em `%edi`, ele destruiria o argumento recebido antes de conseguir copia-lo para `%edx`.

Por isso a ordem correta e:

```text
1. copiar PARAM: %edi -> %edx
2. carregar FIX do parametro 1 em %esi
3. carregar FIX do parametro 0 em %edi
4. chamar a funcao original
```

## Chamada Da Funcao Original

O codigo gerado usa uma chamada indireta:

```asm
movabs endereco_da_funcao, %rax
call *%rax
```

Isso segue a dica do enunciado. Em vez de calcular um deslocamento relativo para `call e8`, o endereco da funcao original e colocado em `%rax` e a chamada e feita por registrador.

## Sugestao De Mais Testes

Depois de testar o `teste.c` atual, crie casos menores para validar cada origem:

```c
/* PARAM */
params[0].tipo_val = INT_PAR;
params[0].orig_val = PARAM;

/* FIX */
params[0].tipo_val = INT_PAR;
params[0].orig_val = FIX;
params[0].valor.v_int = 7;

/* IND */
params[0].tipo_val = INT_PAR;
params[0].orig_val = IND;
params[0].valor.v_ptr = &alguma_variavel;
```

Tambem teste combinacoes como:

```text
PARAM FIX
FIX PARAM
IND FIX
FIX FIX PARAM
FIX PARAM PARAM

coisas a fazer:
- mudar nome da variavel modrm
- padronizar emissao de bytes (criar funcao aux que usa um for)
- fazer verificacoes de tipo e  de tamanho visando proteger o codigo de comportamentos inesperados e outputs errados
-pedir pra ia gerar varios casos teste e tentar quebrarn nosso codigo
PARAM FIX PARAM
```

Esses casos verificam se os registradores estao sendo reorganizados corretamente antes da chamada da funcao original.
