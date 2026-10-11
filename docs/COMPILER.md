# COMPILER.md — Identificação do toolchain original (Fase 2.0)

> Objetivo: saber que compilador/opções produziram o EXE boot, para (a)
> documentar o código que estamos decompilando e (b) permitir, no futuro,
> decompilação casada *de verdade* (C -> MIPS byte-idêntico).

## Evidências coletadas (2026-10-10)

| Evidência | Valor | Implicação |
|---|---|---|
| Strings de lib Sony | `$Id: intr.c,v 1.75 1997/02/07` · `$Id: bios.c,v 1.86 1997/03/28` · `$Id: sys.c,v 1.140 1998/01/12` | Psy-Q (SDK Sony/SN Systems), bibliotecas de 1997–1998 |
| Build stamp | `CAPCOM-Jul 15 1999-15:52:53-CAPCOM` @ `0x8009AC48` | build final de 15/07/1999 (coerente com lançamento EUA set/1999) |
| ISA | MIPS I (R3000A); nenhuma instrução MIPS II/special2 detectada na análise | alvo padrão PS1 |
| Lib incluída estática | GPU (gpu.c/sys.c), CD (bios.c/cd.c), intr (intr.c), SPU, PAD — todas com strings de debug | libs Psy-Q linkadas estaticamente no EXE |
| Idiomas do jogo | C (sem exceções C++/vtables detectadas na análise) | compilador C |

## Conclusão

**Compilador: GCC da família Psy-Q (cc1 MIPS little-endian), otimização
~-O2, MIPS I — versão exata ainda não pinada** (candidatos reais da era:
2.7.2 / 2.8.1, distribuídos com o Psy-Q 4.x que acompanha as versões de
biblioteca acima).

O pinning exato (por comparação de idiomas de codegen contra builds de
referência de cada versão do GCC) **não bloqueia o fluxo deste projeto**:
o critério de "matched" adotado é o *recomp-matched* (equivalência
arquitetural verificada por teste diferencial — ver MATCHED.md), que não
depende de recompilar para MIPS. O pinning só se torna necessário se o
projeto um dia quiser regenerar o disco/EXE a partir do C (true-matched de
imagem completa).

## Referência de convenções observadas (útil para ler o C gerado)

- Prologue: `addiu sp,sp,-N; sw ra,N-4(sp); sw s0..,N-8..(sp)`
- Chamadas: convenção o32 MIPS (`a0-a3` args, `v0` retorno, `t` regs
  caller-saved) — o recompilador preserva isso no C gerado.
- Delay slots executam SEMPRE (o gerado emite o delay explicitamente —
  cuidado com registradores sobrescritos nele; já pegou um bug real no
  teste diferencial de EventFlag_Set, ver MATCHED.md).
