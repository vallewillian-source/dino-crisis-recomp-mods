# MATCHED.md — Critério de decompilação casada do projeto (Fase 2.1)

> "Matched" aqui = uma função reescrita em C legível, comportamentalmente
> idêntica ao original, VERIFICADA por teste, rodando no lugar do C gerado.
> Não é (ainda) o "matched" de decomp clássico (C -> MIPS byte-idêntico);
> a diferença e o porquê estão abaixo.

## Dois tiers de matched

**Tier A — recomp-matched (adotado).** A função original é traduzida do MIPS
pelo PSXRecomp; nós a reescrevemos em C legível que roda no host com a MESMA
assinatura do C gerado (`void func_XXXXXXXX(CPUState* cpu)`). A equivalência
é arquitetural (registradores/RAM/timing na saída), verificada por teste
diferencial. Não dependemos de toolchain MIPS (impossível de obter
legalmente para o cc do Psy-Q).

**Tier B — true-matched (futuro, não bloqueia).** C que compila para MIPS
byte-idêntico ao original (precisa do GCC da época pinado — ver
COMPILER.md). Só é necessário se quisermos reconstruir o EXE/disco a partir
do C. O Tier A é a moeda do projeto; funções Tier A podem ser promovidas a
Tier B depois, sem retrabalho de análise.

## Contrato de um segmento substituído

O C gerado não é "função normal": cada segmento é um nó do despachante CPS
com retomada por `cpu->pc` (interrupções fatiam execução em blocos). Uma
substituição precisa:

1. **Mesmos efeitos arquiteturais finais** — GPRs (incluindo temporários que
   o gerado deixa sujo), RAM, ordem dos stores.
2. **Mesmos accessors, na mesma ordem** — `psx_cyc_load_word(cpu, addr, reg,
   mask)` com os mesmos argumentos (o helper cuida de interlock/load-delay),
   `cpu->write_word`/`write_half` com `g_debug_last_store_pc` +
   `psx_store_cycle_barrier()` antes.
3. **Mesmas máscaras `psx_cyc_step`** (dep+res de registrador), copiadas
   verbatim, sob o mesmo `#ifdef PSX_ENABLE_BLOCK_CYCLES` — timing exato.
4. **Mesmos pontos de saída CPS** — `cpu->pc = destino; return;` incluindo
   splits (o caminho "taken" pode ser OUTRO segmento no dispatch).
5. **Delay slots sempre executam** — já pegou um bug real: o `lui v0,0x800B`
   no delay slot do `bne` do EventFlag_Set sobrescreve o `li v0,2` mesmo com
   branch tomado (o teste diferencial detectou; ver histórico abaixo).

Exclusões conscientes (grau "mod"): sem fatiamento de interrupção DENTRO do
segmento (a interrupção é entregue no próximo boundary — poucas dezenas de
instruções depois); sem hooks de cosim/debug/PGXP.

## Verificação (gates)

1. **psx-matched-test (gate principal, roda no build)** — teste diferencial
   em `src_game/matched_test.c`: para ~30.000 casos fuzzados, executa o
   segmento ORIGINAL (extraído verbatim dos shards por
   `tools/filter_generated.py --mode extract`, renomeado via `-D`) e a
   SUBSTITUIÇÃO sobre o mesmo estado inicial (RAM fake 2 MB + tabela de
   flags sintética), e exige igualdade de GPRs, `pc` final, RAM final e
   sequência exata de stores. O trampolim do teste encadeia os segmentos
   como o dispatch real.
2. **Smoke test do jogo** — build do runtime com os shards FILTRADOS +
   substituições linkadas; jogo deve iniciar e rodar como antes.
3. **Cosim (oracle de ouro, futuro)** — o framework tem co-simulação
   compilado-vs-intérprete (`psxrecomp/tools/cosim.py`, `runtime/src/cosim.c`)
   que encontra a primeira divergência por icount. Segmentos substituídos
   hoje NÃO creditam aposentadoria de instrução (`cosim_instr`) 1:1, então
   ficam FORA do cosim até que alguém implemente crédito por instrução
   espelhando o fluxo original. Documentado como limitação, não bloqueio.

## Mecanismo de build

```
src_game/replacements.txt   # PCs dos segmentos substituídos (fonte única)
src_game/event_flags.c      # C legível (mesmo símbolo/assinatura)
tools/filter_generated.py   # remove os originais -> build/gen-src/ (configure)
                            # + extrai os originais p/ o teste
build/gen-src/              # shards compilados pelo psx-runtime (GEN_FULL_GLOB)
build/matched-test/orig_extracted.c  # originais p/ o teste (-D rename)
```

Fluxo para substituir uma nova função: decompilar no Ghidra → escrever em
`src_game/` seguindo o contrato → acrescentar o PC em `replacements.txt` →
reconfigure (`cmake -B build`) → `ninja psx-matched-test` tem que passar →
`ninja` (jogo) → smoke test. Regenerar o jogo (`generate`) não quebra: o
filtro roda sobre a saída nova (exige reconfigure, regra já existente).

## Histórico

| Data | Função | Resultado |
|---|---|---|
| 2026-10-10 | `EventFlag_Get` (0x8004A87C) | matched, 30k casos OK |
| 2026-10-10 | `EventFlag_Set` entrada (0x8004A7E8) + path de store (0x8004A830) | matched após correção de delay slot achada PELO TESTE (lui v0,0x800B no delay do bne sobrescreve li v0,2) — 30k casos OK |

Estado da área "event flags/tasks": Get/Set matched. Próximos candidatos
naturais: `Task_Clear` (0x8002080C — cauda CPS com jal, exige encadeamento
no teste), `Task_SetStart` (0x80020750).
