/*
 * matched_test.c — Fase 2: teste diferencial de equivalência.
 *
 * Compara, sobre entruras fuzzadas, o segmento ORIGINAL (C gerado, extraído
 * por tools/filter_generated.py --mode extract e renomeado via -D) contra a
 * SUBSTITUIÇÃO legível (src_game/event_flags.c). Equivalência exigida em:
 *   - todos os GPRs (1..31) e cpu->pc final,
 *   - conteúdo da RAM (2 MB),
 *   - sequência exata de stores (endereço, valor, largura).
 *
 * Compila SEM PSX_ENABLE_BLOCK_CYCLES: o gate é arquitetural. As máscaras de
 * ciclo das substituições são cópias verbatim do gerado (mesmo #ifdef) e
 * rodam no jogo. Veja docs/MATCHED.md para os critérios completos.
 */

#include "SLUS_009.22_decls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- originais (extraídos, renomeados via -D no compile do .c) ---- */
extern void orig_8004A87C(CPUState* cpu);
extern void orig_8004A7E8(CPUState* cpu);
extern void orig_8004A830(CPUState* cpu);
/* ---- substituições (src_game/event_flags.c) ----------------------- */
extern void func_8004A87C(CPUState* cpu);
extern void func_8004A7E8(CPUState* cpu);
extern void func_8004A830(CPUState* cpu);

/* ---- globals que o C gerado referencia ----------------------------- */
uint32_t g_debug_last_store_pc;
static uint32_t rd_word(uint32_t a);
volatile uint32_t g_psx_last_fn_entry;
uint32_t psx_read_word(uint32_t addr) { return rd_word(addr); }

/* ---- RAM fake 2 MB + log de stores ---------------------------------- */
static uint8_t  ram[0x200000];        /* working                        */
static uint8_t  ram_pristine[0x200000]; /* entrada do caso              */
static uint8_t  ram_after[0x200000];  /* saída do original (referência) */
typedef struct { uint32_t addr, val; int width; } StoreRec;
static StoreRec stores[64], stores_ref[64];
static int n_stores, n_stores_ref;

static uint32_t phys(uint32_t addr) { return addr & 0x1FFFFFu; }
static uint32_t rd_word(uint32_t a) { uint32_t v; memcpy(&v, ram + phys(a), 4); return v; }
static void wr_word(uint32_t a, uint32_t v)
{
    memcpy(ram + phys(a), &v, 4);
    if (n_stores < 64) { stores[n_stores++] = (StoreRec){ a, v, 32 }; }
}
static uint16_t rd_half(uint32_t a) { uint16_t v; memcpy(&v, ram + phys(a), 2); return v; }
static void wr_half(uint32_t a, uint16_t v)
{
    memcpy(ram + phys(a), &v, 2);
    if (n_stores < 64) { stores[n_stores++] = (StoreRec){ a, v, 16 }; }
}

/* ---- stubs do runtime que os shards referenciam --------------------- */
void psx_check_interrupts_at(CPUState* cpu, uint32_t resume_pc) { (void)cpu; (void)resume_pc; }
void psx_mod_function_entry(CPUState* cpu, uint32_t address) { (void)cpu; (void)address; }
int  psx_datashard_enter(CPUState* cpu, uint32_t key) { (void)cpu; (void)key; return 0; }
void psx_datashard_ret(CPUState* cpu) { (void)cpu; }
int  psx_game_text_native_ok(uint32_t addr) { (void)addr; return 1; }
uint32_t psx_cyc_load_word_slow(CPUState* cpu, uint32_t addr, uint32_t rt, uint32_t reg_mask)
{ (void)cpu; (void)rt; (void)reg_mask; return rd_word(addr); }
int psx_slice_block_impl(CPUState* cpu, uint32_t block_addr, uint32_t bcyc, int side_effects)
{ (void)cpu; (void)block_addr; (void)bcyc; (void)side_effects; return 0; }

#define RA_TEST 0xDEAD0000u

static void reset_state(CPUState* cpu)
{
    memset(cpu, 0, sizeof(*cpu));
    memcpy(ram, ram_pristine, sizeof(ram));
    n_stores = 0;
    cpu->read_word  = rd_word;
    cpu->write_word = wr_word;
    cpu->read_half  = rd_half;
    cpu->write_half = wr_half;
    cpu->gpr[31] = RA_TEST;
}

/* trampolim mínimo: encadeia os segmentos como o dispatch real faz */
static void run_chain(CPUState* cpu, void (*seg_a)(CPUState*), void (*seg_b)(CPUState*))
{
    cpu->pc = 0;
    seg_a(cpu);
    if (cpu->pc == 0x8004A830u) {
        cpu->pc = 0;
        seg_b(cpu);
    }
}

static uint32_t rng_state = 0x12345678u;
static uint32_t xr(void)
{
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;  return rng_state;
}

int main(void)
{
    CPUState cpu;
    int fails = 0, cases = 0;
    uint32_t pre7, pre8;

    /* tabela de grupos: 64 ponteiros -> arrays de 16 words em 0x80100000 */
    for (int g = 0; g < 64; g++) {
        uint32_t ptr = 0x80100000u + (uint32_t)g * 64u;
        memcpy(ram_pristine + phys(0x8009CD18u + (uint32_t)g * 8u), &ptr, 4);
    }

    for (int iter = 0; iter < 30000; iter++) {
        uint32_t group = xr() % 64;
        uint32_t bit   = xr() % 256;
        uint32_t val   = (iter % 3 == 0) ? 0u : 1u;
        uint32_t mode  = (uint32_t)iter % 4;
        /* 0/1: EventFlag_Set completo (chain); 2: Get; 3: segmento B direto */

        /* words dos grupos: conteúdo aleatório por caso */
        for (int g = 0; g < 64; g++)
            for (int w = 0; w < 16; w++) {
                uint32_t v = xr();
                memcpy(ram_pristine + phys(0x80100000u + (uint32_t)g * 64u +
                                           (uint32_t)w * 4u), &v, 4);
            }

        /* ---- executa ORIGINAL ---- */
        reset_state(&cpu);
        pre7 = xr(); pre8 = xr();
        cpu.gpr[4] = group; cpu.gpr[5] = bit; cpu.gpr[6] = val;
        cpu.gpr[7] = pre7;  cpu.gpr[8] = pre8;
        if (mode == 2) {
            orig_8004A87C(&cpu);
        } else if (mode == 3) {
            cpu.gpr[7] = 0x80100000u + (group % 8u) * 64u + (bit % 16u) * 4u;
            cpu.gpr[8] = group % 4u;
            orig_8004A830(&cpu);
        } else {
            run_chain(&cpu, orig_8004A7E8, orig_8004A830);
        }
        memcpy(ram_after, ram, sizeof(ram));
        memcpy(stores_ref, stores, sizeof(StoreRec) * n_stores);
        n_stores_ref = n_stores;
        uint32_t g_ref[32], pc_ref;
        memcpy(g_ref, cpu.gpr, sizeof(g_ref)); pc_ref = cpu.pc;

        /* ---- executa SUBSTITUIÇÃO (mesmo estado inicial) ---- */
        reset_state(&cpu);
        cpu.gpr[4] = group; cpu.gpr[5] = bit; cpu.gpr[6] = val;
        cpu.gpr[7] = pre7;  cpu.gpr[8] = pre8;
        if (mode == 2) {
            func_8004A87C(&cpu);
        } else if (mode == 3) {
            cpu.gpr[7] = 0x80100000u + (group % 8u) * 64u + (bit % 16u) * 4u;
            cpu.gpr[8] = group % 4u;
            func_8004A830(&cpu);
        } else {
            run_chain(&cpu, func_8004A7E8, func_8004A830);
        }

        /* ---- compara ---- */
        int d = 0;
        if (memcmp(cpu.gpr, g_ref, sizeof(g_ref)) != 0) d = 1;
        else if (cpu.pc != pc_ref) d = 2;
        else if (n_stores != n_stores_ref) d = 3;
        else if (memcmp(stores, stores_ref, sizeof(StoreRec) * n_stores_ref) != 0) d = 4;
        else if (memcmp(ram, ram_after, sizeof(ram)) != 0) d = 5;
        if (d) {
            printf("DIVERGENCIA iter=%d mode=%u group=%u bit=%u val=%u reason=%d\n",
                   iter, mode, group, bit, val, d);
            for (int r = 0; r < 32; r++)
                if (g_ref[r] != cpu.gpr[r])
                    printf("  gpr[%d]: orig=%08x novo=%08x\n", r, g_ref[r], cpu.gpr[r]);
            if (pc_ref != cpu.pc)
                printf("  pc: orig=%08x novo=%08x\n", pc_ref, cpu.pc);
            if (n_stores != n_stores_ref)
                printf("  stores: orig=%d novo=%d\n", n_stores_ref, n_stores);
            else
                for (int s = 0; s < n_stores; s++)
                    if (stores[s].addr != stores_ref[s].addr ||
                        stores[s].val != stores_ref[s].val ||
                        stores[s].width != stores_ref[s].width)
                        printf("  store[%d]: orig=(%08x,%08x,w%d) novo=(%08x,%08x,w%d)\n",
                               s, stores_ref[s].addr, stores_ref[s].val,
                               stores_ref[s].width, stores[s].addr,
                               stores[s].val, stores[s].width);
            if (++fails > 3) break;
        }
        cases++;
    }
    printf("psx-matched-test: %d casos, %d divergencias — %s\n",
           cases, fails, fails ? "FALHOU" : "OK");
    return fails ? 1 : 0;
}
