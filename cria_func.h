// cria_func.h
#ifndef CRIA_FUNC_H
#define CRIA_FUNC_H

#include <stdio.h>

typedef enum
{
  INT_PAR,
  PTR_PAR
} TipoValor;

typedef enum
{
  PARAM,
  FIX,
  IND
} OrigemValor;

typedef struct
{
  TipoValor tipo_val;
  OrigemValor orig_val;
  union
  {
    int v_int;
    void *v_ptr;
  } valor;
} DescParam;

// Declaração da função
void cria_func(void *f, DescParam params[], int n, unsigned char codigo[]);

#endif
