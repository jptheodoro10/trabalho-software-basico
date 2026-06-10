/* Joao Pedro Lacerda Theodoro 2510968  */
/* Carlos Frederico Sant'ana 2511920*/

#include "cria_func.h"
#include <stdint.h>

static const unsigned char regs[3] = {7, 6, 2}; /* rdi, rsi, rdx */

static void emite1(unsigned char codigo[], int *pos, unsigned char b)
{
  emiteN(codigo,pos,b,1);
}

static void emite4(unsigned char codigo[], int *pos, int32_t v)
{
  emiteN(codigo,pos,v,4);
}

static void emite8(unsigned char codigo[], int *pos, uintptr_t v)
{
  emiteN(codigo,pos,v,8);
}
static void emiteN (unsigned char codigo[], int *pos, uintptr_t v , int n) {
  int i;

  for (i = 0; i < n; i++)
    codigo[(*pos)++] = (unsigned char)((v >> (8 * i)) & 0xff);
}

static void gera_prologo(unsigned char codigo[], int *pos)
{
  emite1(codigo, pos, 0x55); /* push %rbp */
  emite1(codigo, pos, 0x48);
  emite1(codigo, pos, 0x89);
  emite1(codigo, pos, 0xe5); /* mov %rsp,%rbp */
}

static void gera_mov_param(unsigned char codigo[], int *pos, int origem,
                           int destino, TipoValor tipo)
{
  unsigned char modrm;

  if (origem == destino)
    return;

  if (tipo == PTR_PAR)
    emite1(codigo, pos, 0x48);

  modrm = (unsigned char)(0xc0 | (regs[origem] << 3) | regs[destino]);
  emite1(codigo, pos, 0x89);
  emite1(codigo, pos, modrm);
}

static void gera_mov_fix(unsigned char codigo[], int *pos, int destino,
                         DescParam *param)
{
  if (param->tipo_val == INT_PAR)
  {
    emite1(codigo, pos, (unsigned char)(0xb8 + regs[destino]));
    emite4(codigo, pos, (int32_t)param->valor.v_int);
  }
  else
  {
    emite1(codigo, pos, 0x48);
    emite1(codigo, pos, (unsigned char)(0xb8 + regs[destino]));
    emite8(codigo, pos, (uintptr_t)param->valor.v_ptr);
  }
}

static void gera_mov_ind(unsigned char codigo[], int *pos, int destino,
                         DescParam *param)
{
  unsigned char modrm;

  emite1(codigo, pos, 0x49);
  emite1(codigo, pos, 0xbb); /* movabs endereco,%r11 */
  emite8(codigo, pos, (uintptr_t)param->valor.v_ptr);

  emite1(codigo, pos, param->tipo_val == PTR_PAR ? 0x49 : 0x41);
  emite1(codigo, pos, 0x8b);
  modrm = (unsigned char)((regs[destino] << 3) | 3);
  emite1(codigo, pos, modrm); /* mov (%r11),destino */
}

static void gera_chamada(unsigned char codigo[], int *pos, void *f)
{
  emite1(codigo, pos, 0x48);
  emite1(codigo, pos, 0xb8); /* movabs f,%rax */
  emite8(codigo, pos, (uintptr_t)f);
  emite1(codigo, pos, 0xff);
  emite1(codigo, pos, 0xd0); /* call *%rax */
}

static void gera_epilogo(unsigned char codigo[], int *pos)
{
  emite1(codigo, pos, 0xc9); /* leave */
  emite1(codigo, pos, 0xc3); /* ret */
}

void cria_func(void *f, DescParam params[], int n, unsigned char codigo[])
{
  int pos = 0;
  int origem_param[3];
  int prox_param = 0;
  int i;

  for (i = 0; i < n; i++)
  {
    if (params[i].orig_val == PARAM)
      origem_param[i] = prox_param++;
    else
      origem_param[i] = -1;
  }

  gera_prologo(codigo, &pos);

  for (i = n - 1; i >= 0; i--)
  {
    if (params[i].orig_val == PARAM)
      gera_mov_param(codigo, &pos, origem_param[i], i, params[i].tipo_val);
    else if (params[i].orig_val == FIX)
      gera_mov_fix(codigo, &pos, i, &params[i]);
    else
      gera_mov_ind(codigo, &pos, i, &params[i]);
  }

  gera_chamada(codigo, &pos, f);
  gera_epilogo(codigo, &pos);
}
