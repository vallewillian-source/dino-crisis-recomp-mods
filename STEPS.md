# STEPS.md — Plano de decompilação progressiva do Dino Crisis Recompiled

> **Objetivo de longo prazo:** transformar este projeto de "um binário recompilado
> ilegível" em "um jogo com código-fonte progressivamente legível e modificável",
> priorizando as áreas de gameplay que interessam antes das que não interessam.
>
> Este arquivo é o mapa do projeto. Atualize-o a cada fase concluída — o status
> de cada item vive aqui, não na memória de ninguém.

---

## Estado atual (2026-10-10)

- **Build funcional**: `build/Dino_Crisis_Recompiled` roda o jogo a partir do
  dump canônico deste projeto.
- **Dump canônico**: *Dino Crisis (USA) v1.0 (Rev 0)* do usuário — **não** a Rev 1
  do autor original. Track 1: 379.984.416 bytes, MD5 `4990bb2a9e3374a234885ac983a9397a`.
  As listas `known_*` em `game.toml [prepare_disc]` também aceitam a variante
  mesclada (bin único, 417.381.216 bytes, MD5 `f770fa6e...`).
- **BIOS**: SCPH-5502 do usuário (`~/Downloads/bios-extract/SCPH5502.bin`,
  SHA-256 `9e1f8fb4...` — diferente da documentada, mas o código não impõe hash;
  região de kernel é byte-idêntica ao SCPH-1001, o que o `kernel_bless` verifica).
  Compilada dentro do binário como backend `SCPH5552`.
- **Código gerado**: 43 shards (~1,65 M linhas de C) + dispatch com **2.879 funções**,
  em `generated/`. Não é fonte legível — é tradução mecânica do MIPS do disco.
- **Seeds**: `seeds/ghidra_funcs.txt` — 909 endereços de função (entry + JALs do
  EXE boot), regenerados via `probe_disc.py`.
- **Aviso**: a v1.0 nunca teve validação de gameplay pelo autor original; a
  recompilação roda, mas comportamento anômalo pode ser herança do retarget.

### Como recompilar depois de qualquer mudança

```bash
cd ~/Documentos/Recompilations/dino-crisis-recomp/repo

# Mudanças em generated/ ou runtime: só relinkar (rápido)
ninja -C build

# Mudanças que adicionam/removem arquivos em generated/: reconfigurar antes
# (os globs só entram no build.ninja no configure)
cmake -B build && ninja -C build

# Rodar (o disc.cfg ao lado do exe aponta o CUE canônico)
./build/Dino_Crisis_Recompiled
```

---

## O modelo mental (ler antes de mexer)

O C em `generated/` é um **transcript do código de máquina**, não fonte: funções
com nome de endereço, sem variáveis nomeadas. Decompilação progressiva = trocar
funções desse transcript por versões legíveis que compilam **byte-idênticas** ao
original. Nunca é preciso cobrir as 2.879 — só as áreas que se quer modificar.

Degraus (do barato ao completo):

| Degrau | O que é | Custo | Ganho |
|---|---|---|---|
| 0 | Renomear funções (`symbols.toml` → `psx_symbols.h`) | horas | código navegável |
| 1 | Decompilação casada (matched) de funções-chave | dias/semana por área | fonte modificável de verdade |
| 2 | Mod packages (patch em runtime, sem tocar no C gerado) | baixo | mods versionáveis |

---

## Fase 0 — Infraestrutura de análise [✅ CONCLUÍDA 2026-10-10]

Meta: conseguir abrir o jogo num desassemblador com os seeds carregados e ter o
pipeline de símbolos ativo no build.

- [x] **0.1 — Ghidra instalado**: Ghidra 12.1.3 em `~/Tools/ghidra_12.1.3_PUBLIC`
  + JDK 21 Temurin (`~/Tools/jdk21`; o Java 25 do sistema não é aceito)
  + PyGhidra em venv `~/Tools/ghidra-venv` (Python 3.12; scripts .py só rodam
  via PyGhidra no Ghidra 12). ghidra-allegrex instalada mas é loader de PSP —
  PS1 importa como binário cru (ver `docs/ANALYSIS.md`).
- [x] **0.2 — Projeto Ghidra criado**: `ghidra-project/DinoCrisis.gpr` com
  `SLUS_009.22` importado como binário cru em `0x80017800` (header de 0x800
  bytes → texto cai em `0x80018000`), CPU `MIPS:LE:32:default`.
- [x] **0.3 — Seeds/labels aplicados**: `psxrecomp-analyze` (binário compilado
  em `psxrecomp/recompiler/build/`) gerou `analysis/ghidra_import.py` a partir
  dos seeds + descoberta própria; import criou 2.204 funções, e
  `analysis/fix_labels.py` renomeou todas para `func_XXXXXXXX` com plate
  comments de confiança e bookmarks nos sites indiretos não resolvidos.
- [x] **0.4 — `symbols.toml` criado** na raiz: bloco hand-curado no topo
  (`dc_crt_entry` @ `0x8001A1C8`) + 1.955 entradas append-only do analyze
  (`--emit-symbols --min-confidence medium`, notas de assinatura/callers).
  Detalhes em `docs/ANALYSIS.md`.
- [x] **0.5 — `psx_symbols.h` integrado no build**: alvo CMake `psx-symbols`
  regenera o header quando `symbols.toml` muda; `psx-runtime` depende dele e
  tem a raiz do projeto no include path. Header atual: 1.956 símbolos.
- [x] **0.6 — Overlays mapeados** (descoberta importante): só existe **um**
  PS-X EXE no disco; texto recompilado = EXE boot (`0x80018000..0x800B1800`).
  Overlays de código (se existirem) provavelmente moram em `ZNULL.DAT`
  (32.256.000 bytes @ LBA 161.708) e carregam acima de `0xB1800`. Confirmação
  via runtime debug ou varredura estática — pendente para a Fase 1. Detalhes
  e mapa do disco em `docs/ANALYSIS.md`.

Aceitação da fase: ✅ `symbols.toml` na raiz, `psx_symbols.h` no build,
projeto Ghidra com 2.204 funções nomeadas, `docs/ANALYSIS.md` criado.

## Fase 1 — Mapeamento das áreas de interesse [✅ NÚCLEO CONCLUÍDO 2026-10-10]

Meta: nomear as ~100–200 funções que importam para mods de gameplay.

**Entregue (53 funções curadas, evidenciadas e documentadas em
`docs/ANALYSIS.md`):**

- [x] **Núcleo**: `GameMain`, sistema de tasks (`Task_SetStart`/`Task_Switch`/
  `Task_Clear`/`TaskScheduler_Run`), título/menu, loader de cena, FSM do
  gameplay (`GameTask_Run` + sub-FSM em `0x800a8920`).
- [x] **Input/eventos**: `Pad_Update` (analógico+digital), `EventFlag_Set/Get`,
  `Fade_Update`.
- [x] **Pipeline de frame**: `VSync`, Psy-Q (`ResetGraph`, `DrawSync`,
  `ClearImage`, `LoadImage`, `PutDrawEnv`, `PutDispEnv`), `Gfx_SetupScreen`,
  `qsort`, `bzero`, `SquareRoot0` (GTE).
- [x] **Som**: `Sound_Update` (24 canais + 3 streams), `Sound_VolumeRamp`.
- [x] **Save/Load**: `Memcard_WaitCardReady`, `Memcard_MatchSaveHeader`,
  `SaveScreen_SlotScan`, `LoadScreen_SlotLoad`, `SaveTime_Format`; layout de
  save documentado (produto "BASLUS-00922-DINO").
- [x] **Inventário**: `InventoryScreen_Draw`, `Item_GetName`.
- [x] **Opções/resultados**: `OptionsScreen_*`, `GameOverScreen_Update`,
  `ResultsTime_Draw`, `ResultsTarget_Draw`.
- [x] **Overlays — questão FECHADA**: `ZNULL.DAT` é dummy de padding da Capcom
  (cobre exatamente a região do track 2, nunca lido); track 2 é áudio puro;
  **todo o código do jogo está no EXE boot**. Nada a caçar fora do recomp.

**Em aberto (leads documentados em `docs/ANALYSIS.md` → Fase 2):**

- [ ] **IA dos dinossauros** — vive na sub-FSM (`0x800a8920`, 16 estados
  dumpados) + funções de 0x8005xxxx–0x8006xxxx. Requer exploração da sub-FSM.
- [ ] **Danger meter** — sem âncora de string; usar `PSX_DEBUG_TOOLS` (runtime)
  ou varredura da sub-FSM.

Ferramentas criadas em `analysis/`: `decompile.py`, `dump_states.py`,
`apply_symbols.py`, `fix_labels.py` (fluxo: descobrir → decompilar → nomear →
`apply_symbols.py` + `sync_symbols.py`).

## Fase 2 — Decompilação casada das funções prioritárias [🔄 INFRA PRONTA + PRIMEIRO CONJUNTO MATCHED 2026-10-10]

Meta: substituir o transcript por C legível nas funções que se quer modificar.

Pré-requisitos:
- [x] **2.0 — Toolchain identificado** (`docs/COMPILER.md`): GCC da família
  Psy-Q, MIPS I, ~-O2, build CAPCOM de 15/07/1999 (libs Sony 1997-98). Pinagem
  exata do GCC pendente — NÃO bloqueia o fluxo adotado.
- [x] **2.1 — Critério "matched" definido** (`docs/MATCHED.md`): adotado o
  **recomp-matched** (Tier A): reescrita em C legível com a assinatura do C
  gerado, equivalência arquitetural verificada por **teste diferencial**
  (original extraído verbatim vs substituição, 30k casos fuzzados, compara
  GPRs+PC+RAM+sequência de stores). True-matched (Tier B, C->MIPS byte a
  byte) documentado como futuro, não bloqueante.

Infraestrutura entregue:
- [x] `tools/filter_generated.py` — remove segmentos substituídos dos shards
  (build/gen-src, configure-time) e extrai os originais para o teste.
- [x] `src_game/replacements.txt` (fonte única dos PCs) + `src_game/*.c`.
- [x] CMake: GEN_FULL_GLOB/GEN_MARKER apontam p/ gen-src; alvo
  `psx-matched-test` roda no build.

Matched até agora:
- [x] **EventFlag_Get (0x8004A87C)** — 30k casos, 0 divergências.
- [x] **EventFlag_Set entrada (0x8004A7E8) + store (0x8004A830)** — 30k casos,
  0 divergências (o teste pegou um bug real de delay slot durante o processo).
- [x] `symbols.toml`: status "verified" + notas matched; nomes aplicados ao
  Ghidra; jogo rodando com as substituições ativas.

Ciclo para a próxima função (documentado em MATCHED.md):
1. Decompilar no Ghidra → escrever em `src_game/` seguindo o contrato
   (accessors/máscaras verbatim, delay slots, saídas CPS).
2. Acrescentar o PC em `src_game/replacements.txt`.
3. Reconfigure + `ninja psx-matched-test` (deve passar com 0 divergências) +
   `ninja` + smoke test.
4. `symbols.toml` verified + `docs/ANALYSIS.md`.

Fila (ordem de valor): Task_Clear (0x8002080C) → Task_SetStart (0x80020750) →
restante da área core → inventário → save/load.

Aceitação da fase (total): uma área de gameplay completa em C legível matched.
Área "event flags" ✅; core/tasks em progresso; inventário e save/load
pendentes.

## Fase 3 — Mods e personalização [✅ CONCLUÍDA 2026-10-10]

Meta: implementar as modificações desejadas em cima das fases 1–2.

- [x] **3.1 — Sistema de mods inventariado**: pacotes declarativos
  (`manifest.toml` v5: features/options/patches) + plugins nativos registrados
  em C. Patches `main_exe` = endereço de guest + bytes `expected` (guarda) +
  `replace` estático ou dinâmico via opção (`u8/u16le/u32le/mips_lui_ori_u32`),
  condicionais `when`/`when_integer`. Writes caem via `psx_write_byte` na RAM
  viva + `dirty_ram_mark_executable_range` — o guard stale-static roteia os
  blocos patcheados ao dirty-RAM interpreter. Pegadinhas: scans falham em
  SILÊNCIO (chave é `address`, não `location`, p/ main_exe);
  `build/mods/packages` é LIMPO e re-staged a cada build (pacotes do jogo
  precisam de POST_BUILD próprio); `state.toml` (v2) é user state que
  sobrevive, mas o runtime o REESCREVE e descarta valores de pacotes
  desconhecidos — valores de opção vivem no `[[feature]]`, não no
  `[[package]]`.
- [x] **3.2 — Primeiro mod package**: `dc.enhancement.fade-speed`
  (`repo/mods/packages/`, staged pelo CMake do jogo). Ajusta a velocidade dos
  fades (patch nos addiu do Fade_Update: 0x8004714C / 0x80047230), opção
  "speed" 1x..32x, default_enabled=false. Validado end-to-end: plano aplicado
  ("applied mod plan"), bytes 8x confirmados na RAM viva (30 00 42 24 /
  60 ff 42 24). Mods builtin do framework (cd-speed/fast-loading) permanecem
  DESLIGADOS — quebraram o load de cena (causa da tela preta de 2026-10-10).
- [x] **3.3 — Política** (abaixo).
- [x] **3.4 — Caminho para mods maiores**: mudanças estruturais = funções
  matched em `src_game/` (recomp-matched, teste diferencial); ajustes
  pontuais/visuais = patches declarativos no catálogo do jogo; recursos = 
  overlays de disco (`ModPatchTarget::DiscRaw`).

### Política mods vs. src_game (3.3)

| Tipo de mudança | Veículo | Por quê |
|---|---|---|
| Ajuste pontual de constantes/comportamento observable | mod package declarativo (`repo/mods/packages/`) | versionável, ativável pela UI, sem rebuild |
| Mudança estrutural em função analisada | função matched em `src_game/` (Fase 2) | legível, testável, compilada nativa |
| Conteúdo no disco (vídeo, XA, dados) | overlay/derived-disc no manifest | o runtime aplica no stream do disco |

Regras: (1) todo mod game-owned nasce `default_enabled = false`; (2) NUNCA
patchear por manifest uma função que esteja em `src_game/replacements.txt`
(o patch seria inerte — o corpo roda como C host, não como bytes guest);
nessa casos, mude a função matched diretamente; (3) documentar o alvo e a
evidência no cabeçalho do manifest.

---

## Guarda-chuvas (não esquecer)

- **Dump canônico**: qualquer novo CUE/BIN só entra no projeto depois de bater
  com as listas `known_*` do `game.toml` (validar antes com
  `psxrecomp_cli.py verify-disc`). Os hashes estão neste arquivo na seção
  "Estado atual".
- **Distribuição**: o código gerado deriva do jogo da Capcom — mods e builds
  são para uso pessoal/estudo; nunca versionar o disco, BIOS ou saves.
- **`git`**: o repo tem submódulos (`psxrecomp`, `recomp-ui`) — commits que
  mexem em submódulo exigem cuidado (`git status` mostra `modified content`).
- **ccache**: instalar (`dnf install ccache`) acelera relinks de `generated/`
  após operações de git (o runtime.cmake detecta automaticamente).

## Registro de decisões

| Data | Decisão |
|---|---|
| 2026-10-10 | Projeto retargetizado para o dump v1.0 (Rev 0) do usuário; Rev 1 descartada como canônica. Fluxo sem wizard documentado na memória do projeto. |
| 2026-10-10 | Fase 0 concluída. Stack de análise: Ghidra 12.1.3 + PyGhidra (venv py3.12) + JDK 21 Temurin; PS1 importa como binário cru @ 0x80017800 (ghidra-allegrex é PSP-only). Nomenclatura canônica: `func_XXXXXXXX` (bate com C gerado e symbols.toml); nomes reais entram pelo bloco hand-curado do symbols.toml. |
| 2026-10-10 | Overlays: só o EXE boot está no ISO; provável código dinâmico em `ZNULL.DAT` acima de 0xB1800. Confirmar ANTES de decompilar áreas de gameplay (Fase 1 pode depender de captura de runtime). |
| 2026-10-10 | **Tela preta resolvida** — causa: mods builtin do framework (cd-speed + fast-loading) habilitados em build/mods/state.toml quebravam o load de cena do jogo v1.0 (loader preso em busy; nada a ver com as substituições da Fase 2). Desabilitados; game-owned mods nascem disabled. Fase 3 concluída: sistema de mods inventariado + primeiro mod package (dc.enhancement.fade-speed, escada 1x-32x) validado end-to-end (plano aplicado, bytes 8x na RAM viva). Pegadinhas registradas: chave `address` (não location), scan silencioso, wipe de mods/packages no build, valores de opção no [[feature]] do state.toml v2. |
| 2026-10-10 | Fase 2: adotado o critério **recomp-matched** (Tier A) com teste diferencial como gate; cosim fica como oracle futuro (segmentos substituídos não creditam icount 1:1). Mecanismo: replacements.txt + filter_generated.py + src_game/. Primeira área matched: event flags (Get/Set + store path), 30k casos, 0 divergências; o teste pegou bug real de delay slot. Toolchain original identificado como GCC Psy-Q MIPS I (docs/COMPILER.md). |
| 2026-10-10 | Fase 1 (núcleo) concluída: 53 funções curadas com evidência em 8 áreas. **ZNULL.DAT confirmado como dummy de padding** (extent = região do track 2; nunca lido) — não há código de overlay; o EXE boot contém 100% do código do jogo. IA dos dinos e danger meter ficam para a Fase 2 (leads em docs/ANALYSIS.md). Nomenclatura canônica no symbols.toml (bloco hand-curado) espelhada no projeto Ghidra via apply_symbols.py. |
