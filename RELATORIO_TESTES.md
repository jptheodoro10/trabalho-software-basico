# Relatorio de Testes da `cria_func`

## 1. Objetivo

Este documento descreve os testes realizados sobre a funcao:

```c
void cria_func(void *f, DescParam params[], int n, unsigned char codigo[]);
```

O objetivo foi tentar encontrar erros na geracao de codigo de maquina x86-64,
principalmente:

- passagem de argumentos nos registradores incorretos;
- perda de argumentos durante a reorganizacao dos registradores;
- confusao entre valores inteiros e ponteiros;
- tratamento incorreto de `PARAM`, `FIX` ou `IND`;
- leitura de valores antigos em parametros `IND`;
- alteracao indevida de valores `FIX`;
- interferencia entre funcoes geradas;
- escrita alem da area reservada para o codigo.

Os testes foram implementados no arquivo `teste_completo.c`.

## 2. Ambiente de execucao

O teste foi executado em um ambiente Linux x86-64.

Essa arquitetura e necessaria porque `cria_func` gera diretamente instrucoes
para a ABI System V AMD64. Nessa ABI, os tres primeiros argumentos inteiros ou
ponteiros sao passados nos seguintes registradores:

| Indice do argumento | Inteiro | Ponteiro |
|---|---|---|
| 0 | `%edi` | `%rdi` |
| 1 | `%esi` | `%rsi` |
| 2 | `%edx` | `%rdx` |

O vetor usado para armazenar o codigo foi alocado com `mmap` e permissoes de
leitura, escrita e execucao. Assim, o teste nao depende de tornar a pilha
executavel.

## 3. Premissas

Os testes adotam as seguintes premissas:

1. O valor de `n` esta entre 1 e 3.
2. A funcao original possui exatamente `n` parametros.
3. Cada descritor possui valores validos de `TipoValor` e `OrigemValor`.
4. Quando a origem e `IND`, `valor.v_ptr` aponta para memoria valida.
5. A funcao gerada recebe exatamente um argumento para cada descritor `PARAM`.
6. Os argumentos da funcao gerada aparecem na mesma ordem dos descritores
   `PARAM` encontrados no vetor.
7. A assinatura usada para chamar a funcao gerada corresponde aos seus
   parametros `PARAM`.
8. O buffer fornecido para o codigo possui espaco suficiente.
9. O teste nao tenta validar entradas invalidas, como `n > 3`, ponteiros
   invalidos ou descritores inconsistentes.

Essas premissas seguem a regra de que as entradas fornecidas ao trabalho serao
corretas.

## 4. Estrategia de verificacao

Foram criadas funcoes-alvo para todas as assinaturas possiveis de um a tres
parametros `INT_PAR` e `PTR_PAR`.

As funcoes-alvo registram separadamente cada argumento recebido. Isso permite
detectar trocas de ordem. Uma verificacao baseada somente em soma ou
multiplicacao poderia esconder esse tipo de erro, pois essas operacoes podem
produzir o mesmo resultado com os argumentos trocados.

Para cada chamada, o teste verifica:

- quantidade de argumentos recebidos;
- valor exato de cada argumento;
- ordem dos argumentos;
- valor retornado pela funcao original.

## 5. Enumeracao exaustiva

Cada parametro pode possuir uma das tres origens:

```text
PARAM
FIX
IND
```

Cada parametro tambem pode possuir um dos dois tipos:

```text
INT_PAR
PTR_PAR
```

Foram enumeradas todas as combinacoes validas:

| Numero de parametros | Origens | Tipos | Estruturas |
|---:|---:|---:|---:|
| 1 | `3^1 = 3` | `2^1 = 2` | 6 |
| 2 | `3^2 = 9` | `2^2 = 4` | 36 |
| 3 | `3^3 = 27` | `2^3 = 8` | 216 |
| **Total** | | | **258** |

Isso inclui casos especialmente sensiveis para movimentacao de registradores:

```text
FIX FIX PARAM
FIX PARAM PARAM
IND IND PARAM
IND PARAM PARAM
PARAM FIX PARAM
PARAM IND PARAM
```

Por exemplo, em `FIX FIX PARAM`, o unico argumento da funcao gerada chega no
registrador de indice 0, `%edi` ou `%rdi`, mas precisa ser copiado para o
registrador de indice 2, `%edx` ou `%rdx`, antes que os valores `FIX`
sobrescrevam os registradores anteriores.

## 6. Valores testados

### 6.1 Inteiros

Os testes usam valores escolhidos para revelar problemas de sinal, truncamento
e emissao dos quatro bytes:

```text
0
1
-1
2
-2
INT_MAX
INT_MIN
0x12345678
-123456789
```

Cada uma das 258 estruturas foi executada em nove rodadas, variando esses
valores.

### 6.2 Ponteiros

Foram usados:

- ponteiro nulo;
- endereco de variavel global;
- endereco de variavel local;
- endereco de memoria alocada no heap;
- ponteiros diferentes;
- o mesmo ponteiro em varios parametros.

Ponteiros sao importantes para detectar uma movimentacao incorreta de 32 bits,
que perderia a parte superior de um endereco de 64 bits.

## 7. Testes de `PARAM`

Os testes verificam se:

- cada `PARAM` consome exatamente um argumento da funcao gerada;
- `FIX` e `IND` nao consomem argumentos;
- os `PARAM` sao consumidos em ordem compactada;
- o valor chega ao parametro correto da funcao original;
- argumentos nao sao destruidos durante a reorganizacao dos registradores;
- tipos inteiros usam 32 bits e ponteiros usam 64 bits;
- tres parametros podem receber o mesmo ponteiro.

## 8. Testes de `FIX`

Depois de gerar uma funcao, o teste modifica os descritores usados na geracao.
A funcao gerada precisa continuar usando os valores originais.

Isso confirma que:

- o inteiro fixo foi embutido no codigo;
- o ponteiro fixo foi embutido no codigo;
- a funcao gerada nao consulta novamente `params`;
- valores `FIX` nao mudam entre chamadas;
- varios parametros podem conter o mesmo ponteiro fixo.

## 9. Testes de `IND`

Cada caso com `IND` e chamado pelo menos duas vezes.

Entre as chamadas, o conteudo da memoria referenciada e alterado. A segunda
chamada deve receber o novo valor.

Isso verifica que:

- `IND` le a memoria no momento de cada chamada;
- o valor nao foi congelado durante `cria_func`;
- `IND INT_PAR` carrega quatro bytes;
- `IND PTR_PAR` carrega oito bytes;
- a leitura pode retornar `NULL`;
- varios descritores `IND` podem apontar para a mesma variavel;
- uma variavel compartilhada e relida corretamente depois de alterada.

## 10. Retorno e chamadas internas

Foram testados retornos positivos, negativos e valores-limite, incluindo:

```text
0
-1
INT_MIN
INT_MAX
```

As funcoes originais tambem chamam uma funcao auxiliar C para registrar os
argumentos. Isso ajuda a exercitar uma chamada normal feita pela funcao
original depois de ela ser invocada pelo codigo gerado.

## 11. Isolamento e reutilizacao

O teste cria funcoes diferentes em buffers executaveis separados e as chama de
forma intercalada.

Foi verificado que:

- uma funcao gerada nao interfere na outra;
- chamar novamente uma funcao antiga continua produzindo o resultado correto;
- o mesmo buffer pode ser reutilizado para gerar configuracoes diferentes;
- nao existe estado residual relevante entre chamadas.

## 12. Limite de escrita

Foram reservados 256 bytes para cada funcao gerada. Depois dessa area foram
colocados 64 bytes sentinela com o valor `0xA5`.

Depois de cada geracao, os bytes sentinela foram conferidos. Qualquer alteracao
indicaria que `cria_func` escreveu alem do limite considerado pelo teste.

Esse teste detecta escrita depois dos 256 bytes. Ele nao calcula o tamanho
exato da funcao gerada e nao detecta escrita que continue dentro desses 256
bytes, mas alem do ultimo byte realmente necessario.

## 13. Casos pseudoaleatorios

Alem da enumeracao exaustiva das estruturas, foram executados 5.000 casos
pseudoaleatorios.

Cada caso escolhe:

- numero de parametros;
- combinacao de origens;
- combinacao de tipos;
- rodada de valores.

O gerador pseudoaleatorio usa uma semente fixa. Portanto, a sequencia e
reprodutivel: uma falha deve reaparecer ao executar novamente o mesmo teste.

## 14. Resultado obtido

A execucao apresentou:

```text
Estruturas exaustivas: 258
Rodadas exaustivas por estrutura: 9
Casos aleatorios: 5000
Geracoes/intercalacoes executadas: 7331
Chamadas de funcoes geradas: 14653
RESULTADO: todos os testes passaram
```

As 7.331 geracoes e intercalacoes sao formadas principalmente por:

```text
258 estruturas * 9 rodadas = 2322
5000 casos pseudoaleatorios
9 testes adicionais de intercalacao e aliasing
Total = 7331
```

A quantidade de chamadas e maior porque a maioria das funcoes geradas e
executada duas vezes. A segunda chamada verifica principalmente a releitura de
`IND` e a permanencia de `FIX` e `PARAM`.

## 15. Conclusao

Nenhuma falha foi encontrada nos cenarios executados.

O resultado fornece evidencia de que a implementacao:

- reorganiza corretamente os tres primeiros registradores de argumentos;
- preserva a ordem dos parametros;
- diferencia movimentos de 32 e 64 bits;
- implementa corretamente `PARAM`, `FIX` e `IND`;
- rele valores indiretos em chamadas sucessivas;
- preserva valores fixos;
- permite reutilizacao e coexistencia de funcoes geradas;
- respeita o limite de 256 bytes observado pelo teste.

Passar nesses testes aumenta a confianca na implementacao, mas nao constitui
uma prova formal de ausencia de erros. Os resultados valem para as premissas
de entrada correta e para o ambiente Linux x86-64 utilizado.

## 16. Como reproduzir

Em Linux x86-64:

```bash
gcc -Wall -Wextra -O0 cria_func.c teste_completo.c -o teste_completo
./teste_completo
```

O programa retorna:

- codigo `0` quando todos os testes passam;
- codigo `1` quando alguma verificacao falha;
- codigo `2` quando ocorre uma falha de infraestrutura, como erro de `mmap` ou
  `malloc`;
- codigo `77` quando executado fora de Linux x86-64.
