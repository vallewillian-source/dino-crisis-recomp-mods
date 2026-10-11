/*
 * event_flags.c — Fase 2: substituições "matched" do sistema de event flags.
 *
 * Estas funções SUBSTITUEM segmentos do C gerado (mesmo símbolo, mesma
 * assinatura). O filtro build-time (tools/filter_generated.py) remove os
 * originais dos shards; o linkedin escolhe as definições deste arquivo.
 *
 * Contrato de um segmento substituído (ver docs/MATCHED.md):
 *   1. Mesmos efeitos arquiteturais finais: gpr[2..31], RAM via
 *      psx_cyc_load_word / cpu->write_word / cpu->write_half (ordem incluída).
 *   2. Mesmas chamadas de acesso na mesma ordem (load helpers com os mesmos
 *      argumentos — o helper cuida de interlock/delay de carga).
 *   3. psx_cyc_step com as MESMAS máscaras dep+res do gerado (copiadas
 *      verbatim), sob o mesmo #ifdef PSX_ENABLE_BLOCK_CYCLES.
 *   4. Mesmos pontos de saída CPS: cpu->pc = destino; return.
 *      (Sem fatiamento por interrupção DENTRO do segmento — entregue no
 *      próximo boundary; documentado como limitação de grau "mod".)
 *   5. Sem hooks de cosim/debug/PGXP — segmentos substituídos ficam fora do
 *      oracle cosim; a verificação é o teste diferencial (psx-matched-test).
 *
 * Semântica original (decompilada e confirmada por teste):
 *   Tabela de grupos: RAM[0x8009CD18 + group*8] -> ptr para array de words.
 *   flag(group,bit): word = ptr[bit>>5]; testa/liga/desliga (1<<(bit&31)).
 *   Evento especial grupo 1 bit 2: zera 0x800B58B8, 0x800B5898 e os
 *   counters 0x1F801108/0x1F80110A (timer de jogo / freeze de physics).
 */

#include "SLUS_009.22_decls.h"

#ifndef g_debug_last_store_pc
extern uint32_t g_debug_last_store_pc;
#endif

/* ------------------------------------------------------------------ *
 * EventFlag_Get @ 0x8004A87C (60 bytes, 1 bloco, leaf)
 *
 *   v0 = (flag(group, bit) != 0)
 * ------------------------------------------------------------------ */
void func_8004A87C(CPUState* cpu)
{
    const uint32_t group = cpu->gpr[4];   /* a0: índice do grupo      */
    const uint32_t bit   = cpu->gpr[5];   /* a1: índice do bit        */

    /* ptr = RAM[0x8009CD18 + group*8]                        */
    /* word = ptr[bit >> 5]                                   */
    /* v0 = (word & (1 << (bit & 31))) != 0                   */
    const uint32_t bit_in_word = bit & 0x1Fu;
    const uint32_t mask        = 1u << bit_in_word;

#ifdef PSX_ENABLE_BLOCK_CYCLES
    if (psx_slice_block(cpu, 0x8004A87Cu, 15u, 0)) return;
    psx_cyc_step(cpu, 0x10u);   /* sll  a0, a0, 3             */
    psx_cyc_step(cpu, 0x4u);    /* lui  v0, 0x800A            */
    psx_cyc_step(cpu, 0x4u);    /* addiu v0, -13032           */
    psx_cyc_step(cpu, 0x14u);   /* addu a0, a0, v0            */
    psx_cyc_step(cpu, 0x24u);   /* srl  v0, a1, 5             */
    psx_cyc_step(cpu, 0x4u);    /* sll  v0, v0, 2             */
    psx_cyc_step(cpu, 0x20u);   /* andi a1, a1, 0x1F          */
#endif

    const uint32_t group_ptr =
        psx_cyc_load_word(cpu, 0x8009CD18u + (group << 3), 4, 0x10u);

#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_cyc_step(cpu, 0x9u);    /* li   v1, 1                 */
    psx_cyc_step(cpu, 0x14u);   /* addu a0, a0, v0            */
#endif

    const uint32_t word_addr = group_ptr + (bit >> 5) * 4u;
    const uint32_t word      = psx_cyc_load_word(cpu, word_addr, 2, 0x10u);

#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_cyc_step(cpu, 0x28u);   /* sllv v1, v1, a1            */
    psx_cyc_step(cpu, 0xCu);    /* and  v0, v0, v1            */
    psx_cyc_step(cpu, 0x80000001u); /* jr   ra (+bloco)       */
    psx_cyc_step(cpu, 0x5u);    /* sltu v0, zero, v0 (delay)  */
#endif

    /* Estado final de registradores — idêntico ao gerado:        */
    cpu->gpr[4] = word_addr;              /* a0: endereço da word    */
    cpu->gpr[5] = bit_in_word;            /* a1: bit & 31            */
    cpu->gpr[3] = mask;                   /* v1: máscara             */
    cpu->gpr[2] = ((word & mask) != 0u) ? 1u : 0u;  /* v0: resultado  */

    psx_check_interrupts_at(cpu, cpu->gpr[31]);
    cpu->pc = cpu->gpr[31];
}

/* ------------------------------------------------------------------ *
 * EventFlag_Set @ 0x8004A7E8 (segmento de entrada, 72 bytes, 2 blocos)
 *
 *   addr = &flag(group, bit); se val!=0 -> desvia p/ segmento de set
 *   (0x8004A830); senão limpa o bit e grava.
 * ------------------------------------------------------------------ */
void func_8004A7E8(CPUState* cpu)
{
    if (cpu->pc != 0u) {
        uint32_t _cont = cpu->pc; cpu->pc = 0;
        switch (_cont) {
            case 0x8004A810u: goto block_8004A810;   /* continuação   */
            default: break;
        }
    }

    const uint32_t group = cpu->gpr[4];   /* a0 */
    const uint32_t bit   = cpu->gpr[5];   /* a1 */
    const uint32_t val   = cpu->gpr[6];   /* a2 */

#ifdef PSX_ENABLE_BLOCK_CYCLES
    if (psx_slice_block(cpu, 0x8004A7E8u, 10u, 0)) return;
    psx_cyc_step(cpu, 0x111u);  /* move t0, a0                */
    psx_cyc_step(cpu, 0x104u);  /* sll  v0, t0, 3             */
    psx_cyc_step(cpu, 0x8u);    /* lui  v1, 0x800A            */
    psx_cyc_step(cpu, 0x8u);    /* addiu v1, -13032           */
    psx_cyc_step(cpu, 0xCu);    /* addu v0, v0, v1            */
#endif

    const uint32_t group_ptr =
        psx_cyc_load_word(cpu, 0x8009CD18u + (group << 3), 7, 0x4u);

#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_cyc_step(cpu, 0x24u);   /* srl  v0, a1, 5             */
    psx_cyc_step(cpu, 0x4u);    /* sll  v0, v0, 2             */
    psx_cyc_step(cpu, 0x41u);   /* bne  a2, zero ...          */
    /* delay slot: addu a3, a3, v0                               */
    psx_cyc_step(cpu, 0x84u);
#endif

    uint32_t word_addr = group_ptr + ((bit >> 5) * 4u);
    cpu->gpr[7] = word_addr;              /* a3: endereço da word    */
    cpu->gpr[8] = group;                  /* t0: vivo p/ segmento B  */

    if (val != 0u) {
        /* caminho de SET é o segmento 0x8004A830 (peça separada no
         * dispatch); entrega pelo mecanismo CPS normal.             */
        cpu->gpr[2] = (bit >> 5) * 4u;    /* v0 (feito no delay slot)*/
        psx_check_interrupts_at(cpu, 0x8004A830u);
        cpu->pc = 0x8004A830u;
        return;
    }

#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_check_interrupts_at(cpu, 0x8004A810u);
#endif

block_8004A810:
    /* ---- caminho de CLEAR: word &= ~(1 << (bit & 31)) ---- */
#ifdef PSX_ENABLE_BLOCK_CYCLES
    if (psx_slice_block(cpu, 0x8004A810u, 8u, 0)) return;
    psx_cyc_step(cpu, 0x24u);   /* andi v0, a1, 0x1F          */
    psx_cyc_step(cpu, 0x9u);    /* li   v1, 1                 */
    psx_cyc_step(cpu, 0xCu);    /* sllv v1, v1, v0            */
#endif
    const uint32_t clear_mask = ~(1u << (bit & 0x1Fu));
#ifdef PSX_ENABLE_BLOCK_CYCLES
    /* (load do word atual — helper trata interlock)             */
#endif
    const uint32_t word  = psx_cyc_load_word(cpu, word_addr, 2, 0x80u);
#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_cyc_step(cpu, 0x9u);    /* not  v1, v1                */
    psx_cyc_step(cpu, 0xCu);    /* and  v0, v0, v1            */
    psx_cyc_step(cpu, 0x80000001u); /* jr   ra                */
    psx_cyc_step(cpu, 0x84u);   /* sw   v0, 0(a3)  (delay)    */
#endif
    const uint32_t new_word = word & clear_mask;
    g_debug_last_store_pc = 0x8004A82Cu;
    psx_store_cycle_barrier();
    cpu->write_word(word_addr, new_word);

    cpu->gpr[2] = new_word;               /* v0 final                */
    cpu->gpr[3] = clear_mask;             /* v1 final                */
    psx_check_interrupts_at(cpu, cpu->gpr[31]);
    cpu->pc = cpu->gpr[31];
}

/* ------------------------------------------------------------------ *
 * EventFlag_Set @ 0x8004A830 (segmento de SET, 76 bytes, 4 blocos)
 *
 *   word |= (1 << (bit & 31)); grava.
 *   Evento especial (group==1 && bit==2): zera 0x800B58B8 (physics),
 *   0x800B5898 e counters 0x1F801108/0x1F80110A.
 *   Entrada: a3 (gpr7) = addr da word, t0 (gpr8) = group — deixados
 *   pelo segmento de entrada.
 * ------------------------------------------------------------------ */
void func_8004A830(CPUState* cpu)
{
    if (cpu->pc != 0u) {
        uint32_t _cont = cpu->pc; cpu->pc = 0;
        switch (_cont) {
            case 0x8004A84Cu: goto block_8004A84C;
            case 0x8004A858u: goto block_8004A858;
            case 0x8004A874u: goto block_8004A874;
            default: break;
        }
    }

    const uint32_t group = cpu->gpr[8];   /* t0: group (do segmento A) */
    const uint32_t bit   = cpu->gpr[5];   /* a1                        */
    const uint32_t word_addr = cpu->gpr[7];

#ifdef PSX_ENABLE_BLOCK_CYCLES
    if (psx_slice_block(cpu, 0x8004A830u, 7u, 0)) return;
    psx_cyc_step(cpu, 0x24u);   /* andi v0, a1, 0x1F          */
    psx_cyc_step(cpu, 0x11u);   /* li   a0, 1                 */
#endif
    const uint32_t set_mask = 1u << (bit & 0x1Fu);
#ifdef PSX_ENABLE_BLOCK_CYCLES
    /* (load do word atual)                                      */
#endif
    const uint32_t word = psx_cyc_load_word(cpu, word_addr, 3, 0x80u);
#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_cyc_step(cpu, 0x14u);   /* sllv v0, a0, v0            */
    psx_cyc_step(cpu, 0xCu);    /* or   v1, v1, v0            */
    psx_cyc_step(cpu, 0x110u);  /* bne  t0, a0 ...            */
    /* delay slot: sw   v1, 0(a3)                                */
    psx_cyc_step(cpu, 0x88u);
#endif
    const uint32_t new_word = word | set_mask;
    g_debug_last_store_pc = 0x8004A848u;
    psx_store_cycle_barrier();
    cpu->write_word(word_addr, new_word);

    /* Estado final por caminho — espelha o gerado exatamente:      */
    if (group != 1u) {
        /* saída direta do bloco 1: v0=máscara, v1=word|mask, a0=1  */
        cpu->gpr[2] = set_mask;
        cpu->gpr[3] = new_word;
        cpu->gpr[4] = 1u;
#ifdef PSX_ENABLE_BLOCK_CYCLES
        psx_check_interrupts_at(cpu, 0x8004A874u);
#endif
        goto block_8004A874;
    }
#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_check_interrupts_at(cpu, 0x8004A84Cu);
#endif

block_8004A84C:
    /* caso especial group==1: só procede se bit==2              */
#ifdef PSX_ENABLE_BLOCK_CYCLES
    if (psx_slice_block(cpu, 0x8004A84Cu, 3u, 0)) return;
    psx_cyc_step(cpu, 0x5u);    /* li   v0, 2                 */
    psx_cyc_step(cpu, 0x24u);   /* bne  a1, v0 ...            */
    psx_cyc_step(cpu, 0x4u);    /* (delay) lui v0, 0x800B     */
#endif
    if (bit != 2u) {
        /* ATENÇÃO: o `lui v0,0x800B` está no DELAY SLOT do bne — executa
         * mesmo com o branch tomado, sobrescrevendo o li v0,2.           */
        cpu->gpr[2] = 0x800B0000u;
        cpu->gpr[3] = new_word;
        cpu->gpr[4] = 1u;
#ifdef PSX_ENABLE_BLOCK_CYCLES
        psx_check_interrupts_at(cpu, 0x8004A874u);
#endif
        goto block_8004A874;
    }
#ifdef PSX_ENABLE_BLOCK_CYCLES
    psx_check_interrupts_at(cpu, 0x8004A858u);
#endif

block_8004A858:
    /* group==1 && bit==2: reset de física/timer                 */
#ifdef PSX_ENABLE_BLOCK_CYCLES
    if (psx_slice_block(cpu, 0x8004A858u, 7u, 0)) return;
    psx_cyc_step(cpu, 0x5u);
    psx_cyc_step(cpu, 0x4u);
    psx_cyc_step(cpu, 0x2u);
    psx_cyc_step(cpu, 0x3u);
    psx_cyc_step(cpu, 0x2u);
    psx_cyc_step(cpu, 0x3u);
    psx_cyc_step(cpu, 0x5u);
#endif
    g_debug_last_store_pc = 0x8004A858u;
    psx_store_cycle_barrier();
    cpu->write_half(0x800B0000u + 22712u, 0u);   /* 0x800B58B8: physics */
    cpu->write_half(0x1F800000u + 10u,   0u);    /* 0x1F80110A: timer   */
    cpu->write_half(0x1F800000u + 8u,    0u);    /* 0x1F801108: timer   */
    cpu->write_half(0x800B0000u + 22680u, 0u);   /* 0x800B5898          */
    cpu->gpr[2] = 0x800B0000u;            /* v0 (lui no delay slot)  */
    cpu->gpr[1] = 0x1F800000u;            /* at                      */
    cpu->gpr[3] = new_word;
    cpu->gpr[4] = 1u;

block_8004A874:
#ifdef PSX_ENABLE_BLOCK_CYCLES
    if (psx_slice_block(cpu, 0x8004A874u, 2u, 0)) return;
    psx_cyc_step(cpu, 0x80000001u); /* jr   ra                */
    psx_cyc_step(cpu, 0x1u);        /* nop (delay)             */
#endif
    psx_check_interrupts_at(cpu, cpu->gpr[31]);
    cpu->pc = cpu->gpr[31];
}
