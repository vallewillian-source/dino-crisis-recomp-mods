# ANALYSIS.md — Estado da análise (Fase 0)

> Arquivo-vivo da análise do jogo. Atualizar a cada sessão de decompilação.
> O plano geral vive em [../STEPS.md](../STEPS.md).

## Artefatos gerados na Fase 0 (2026-10-10)

| Artefato | O que é | Como regenerar |
|---|---|---|
| `symbols.toml` | Mapa progressivo de símbolos: bloco **hand-curado** no topo (nomes de verdade) + bloco append-only do analyze (1.955 funções com assinatura/ call notes) | `psxrecomp/recompiler/build/psxrecomp-analyze disc/SLUS_009.22 --seeds seeds/ghidra_funcs.txt --symbols symbols.toml --emit-symbols --min-confidence medium --out analysis --emit-ghidra analysis/ghidra_import.py --emit-tsv analysis/functions.tsv --quiet` (append-only; nomes existentes sempre ganham) |
| `psx_symbols.h` | Header C com `PSX_FN_<nome>` + alias `func_<pc>` (1.956 entradas) — consumível por qualquer TU do runtime | automático no build (alvo `psx-symbols` no CMakeLists) ou `python3 psxrecomp/tools/new_project_layout/sync_symbols.py --symbols symbols.toml --header psx_symbols.h` |
| `annotations/SLUS_009.22_annotations.csv` | Notas `(endereço, texto)` que viram comentários `[NOTE]` inline no C gerado pelo emitter | editar direto (formato: `0xADDR, texto livre`) |
| `analysis/ghidra_import.py` | Script oficial do framework p/ Ghidra: cria 2.207 funções com nomes/confiança e bookmarks de sites indiretos | gerado junto com o analyze (flag `--emit-ghidra`) |
| `analysis/analysis.json`, `edges.json`, `indirect.json`, `refs.json`, `functions.tsv` | Bundle da análise estática (grafo de chamadas, sites indiretos, TSV diff-ável) | mesma linha do analyze |
| Projeto Ghidra | `ghidra-project/DinoCrisis.gpr` (não versionado) com `SLUS_009.22` importado (binário cru @ `0x80017800`, MIPS:LE:32) — 2.204 funções nomeadas `func_XXXXXXXX` + plate comments de confiança | ver "Abrindo o Ghidra" abaixo |

## Mapa do disco (dump canônico v1.0)

ISO9660 com **81 arquivos**. Apenas **um** PS-X EXE no disco inteiro:

| Arquivo | LBA | Tamanho | pc0 | t_addr | text |
|---|---|---|---|---|---|
| `SLUS_009.22` | 161.099 | 630.784 | `0x8001A1C8` | `0x80018000` | `0x99800` |

Maiores blocos de dados (não-EXE): `PSX/MOVIE/*.STR` (vídeos), `PSX/DATA_XA/STAGE*.XAS`
(áudio/áudio-XA por estágio), e **`ZNULL.DAT`** (32.256.000 bytes @ LBA 161.708).

## Overlays — descoberta-chave da Fase 0

- O texto recompilado cobre **exatamente** o EXE boot:
  `0x80018000 .. 0x80018000+0x99800 = 0x800B1800`.
- O runtime arma o guardião de overlay em `overlay_region_floor = 0xB1800` — ou
  seja, o motor ESPERA código dinâmico acima do fim do texto do EXE.
- Como não há outro EXE no ISO, os overlays de código (se existirem) quase
  certamente moram **dentro de `ZNULL.DAT`** (padrão Capcom da era) e são
  carregados para endereços fixos acima de `0xB1800` em runtime.

**Consequência para o projeto:** o C gerado hoje cobre só o EXE principal.
Código de overlay executado em runtime roda pelo interpretador fallback
(dirty-RAM interpreter) — funcional, mas lento e fora do C gerado. Confirmar e
mapear os overlays é pré-requisito para decompilar as áreas de gameplay que
viverem neles (possivelmente IA de dinossauro/estágio).

**Como confirmar (próxima sessão):**
1. Rodar o jogo com `PSX_DEBUG_TOOLS=ON` (build Debug/RelWithDebInfo) e capturar
   PCs executados acima de `0x800B1800` (heartbeat/per-block recording).
2. Alternativa estática: varrer `ZNULL.DAT` por blocos com densidade de
   instruções MIPS válidas e prologos de função (padrão `addiu sp,-X / sw ra`).
3. Os seeds de overlay entram em `seeds/ghidra_funcs.txt` + `symbols.toml`
   conforme descobertos (o fluxo do framework já espera isso).

## Abrindo o Ghidra

Setup instalado (Fase 0):

- **Ghidra 12.1.3** em `~/Tools/ghidra_12.1.3_PUBLIC`, rodando com **JDK 21
  Temurin** (`~/Tools/jdk21` — o Java 25 do sistema não é aceito pelo Ghidra).
- **PyGhidra** em venv `~/Tools/ghidra-venv` (Python 3.12 — o 3.14 do Fedora
  não tem wheel de JPype1). O Ghidra 12 removeu o Jython; scripts `.py` só
  rodam via PyGhidra.
- ghidra-allegrex v21.4 instalada mas **irrelevante para PS1** (é loader de
  PSP ELF); manter para referência.

Importação (feita; o programa já está no projeto `repo/ghidra-project/DinoCrisis`):

```bash
JAVA_HOME=~/Tools/jdk21 GHIDRA_INSTALL_DIR=~/Tools/ghidra_12.1.3_PUBLIC \
~/Tools/ghidra-venv/bin/python -m pyghidra.ghidra_launch \
  ghidra.app.util.headless.AnalyzeHeadless \
  ~/Documentos/Recompilations/dino-crisis-recomp/repo/ghidra-project DinoCrisis \
  -import "disc/SLUS_009.22" -overwrite \
  -processor MIPS:LE:32:default -loader BinaryLoader -loader-baseAddr 0x80017800 \
  -scriptPath analysis -postScript ghidra_import.py
```

- `loader-baseAddr 0x80017800`: o PS-X EXE tem header de 0x800 bytes; o texto
  (offset 0x800 do arquivo) precisa cair em `0x80018000`.
- Depois do import, rodar `analysis/fix_labels.py` (renomeia `FUN_` →
  `func_XXXXXXXX` com plate comments de confiança — o script oficial só nomeia
  funções com nome real).

Estado do projeto: **2.204 funções**, todas nomeadas `func_XXXXXXXX`, com
plate comment de confiança e bookmarks nos sites indiretos não resolvidos.

GUI: `JAVA_HOME=~/Tools/jdk21 ~/Tools/ghidra_12.1.3_PUBLIC/ghidraRun` e abrir
`repo/ghidra-project/DinoCrisis.gpr`.

## Funções nomeadas até agora

Bloco **hand-curado** do `symbols.toml` (53 funções, Fase 1 — 2026-10-10).
Fonte da evidência: 777 strings do EXE + `refs.json` (função→dados) +
`edges.json` (call graph) + stubs de BIOS identificados pelo framework +
decompilação Ghidra de ~30 funções (`analysis/decompile.py`). Aplicado no
projeto Ghidra via `analysis/apply_symbols.py`.

### Área 1 — Núcleo / tasks / FSM

| pc | nome | status | evidência |
|---|---|---|---|
| `0x8001CE10` | `GameMain` | analyzed | decompilado: init + loop de cena + loop de frame (contador em G+0x04), VSync, PutDisp/DrawEnv, LoadImage/DrawSync por buffer duplo |
| `0x80020750` / `0x8002086C` / `0x8002080C` | `Task_SetStart` / `Task_Switch` / `Task_Clear` | analyzed | tabela de tasks (stride 0x18) em 0x800B5838; Title→Gameplay |
| `0x80020640` | `TaskScheduler_Run` | analyzed | BIOS OpenThread/ChangeThread |
| `0x80020A40` | `Title_Task` | analyzed | SceneLoad_Start + espera busy |
| `0x80049B80` | `TitleMenu_Update` | analyzed | menu 3 entradas (0x80018674/78/7C) |
| `0x8002A094` / `0x80029654` | `SceneLoad_Start` / `SceneLoad_IsBusy` | analyzed | decompilados |
| `0x800296C0` | `SceneUpdate_Dispatch` | analyzed | dispatch por PTR table indexada |
| `0x80077810` | `GameTask_Run` | analyzed | zera structs (0xB0 + 0x850) e despacha FSM |
| `0x8007787C` / `0x800778CC` / `0x80079BE8` | `GameFsm_Advance` / `GameFsm_DispatchSub` / `GameFsm_EndTask` | analyzed/guessed | FSM em 0x800a8914, sub-FSM em 0x800a8920 |

**Arquitetura revelada:** `dc_crt_entry → GameMain`: loop externo = cena
(clear, setup GPU, seleciona task), loop interno = frame (contador, VSync,
updates, draw). Tasks são handlers substituíveis (título ↔ gameplay).

### Área 2 — Input / eventos / fade

| pc | nome | status |
|---|---|---|
| `0x8001DAEC` | `Pad_Update` (analógico 'A' + digital 's', bitfields 0x800c2550/0x800c2560) | analyzed |
| `0x8004A7E8` / `0x8004A87C` | `EventFlag_Set` / `EventFlag_Get` | analyzed |
| `0x80047030` | `Fade_Update` (FSM 0..3, brilho 0x70→0xFF, quad fullscreen) | analyzed |

### Área 3 — Pipeline de frame (Psy-Q e helpers)

| pc | nome | status |
|---|---|---|
| `0x800878C0` / `0x80087A38` | `VSync` / `VSync_WaitInternal` | analyzed |
| `0x80090450` / `0x800907CC` / `0x80090950` / `0x80090A78` / `0x80090E30` / `0x80090FFC` / `0x800906D4` | `ResetGraph` / `DrawSync` / `ClearImage` / `LoadImage` / `PutDrawEnv` / `PutDispEnv` / `VSyncCallback` | analyzed (strings de debug do lib) / guessed |
| `0x8001CC7C` | `Gfx_SetupScreen` (320×240) | analyzed |
| `0x80076620` / `0x8001FFC0` | `OT_Build` / `Gfx_SubmitTpage` | guessed |
| `0x80095340` / `0x800601A4` / `0x8008EEA0` | `qsort` / `bzero` / `SquareRoot0` (GTE) | analyzed |
| `0x8009458C` / `0x8009461C` / `0x800944FC` | `PadInit` / `PadStart` / `PadStop` | guessed (wrappers BIOS B0:12-15) |

### Área 4 — Som

| pc | nome | status |
|---|---|---|
| `0x8005D798` | `Sound_Update` (24 canais 0x24 + 3 streams 0xEC) | analyzed |
| `0x8005DBF0` | `Sound_VolumeRamp` | analyzed |

### Área 5 — Save/Load (memory card)

| pc | nome | status |
|---|---|---|
| `0x800236C0` | `Memcard_WaitCardReady` (paths `bu00:`/`bu10:`, stubs A0:AB/AC) | analyzed |
| `0x8002419C` | `Memcard_MatchSaveHeader` ("BASLUS-00922-DINO") | analyzed |
| `0x800246A4` | `SaveScreen_SlotScan` (firstfile/nextfile, UI de slots) | analyzed |
| `0x80025748` | `LoadScreen_SlotLoad` (abre arquivo do slot, confirm. UI) | analyzed |
| `0x80025E88` | `SaveTime_Format` ("%02d:%02d:%02d") | analyzed |

**Layout do save:** entradas de 0x28 bytes (dir card) + bloco de 0x80 bytes
por slot; produto "BASLUS-00922-DINO"; strings `NO.%02d`/`NO DATA`.

### Área 6 — Inventário

| pc | nome | status |
|---|---|---|
| `0x8006F494` | `InventoryScreen_Draw` (2 páginas × 5 slots, cursor, 3 abas) | analyzed |
| `0x80071350` | `Item_GetName` | guessed |

### Área 7 — Opções / resultados

| pc | nome | status |
|---|---|---|
| `0x8004C250` | `OptionsScreen_Update` (KEY CONFIG/SOUND/BGM VOLUME) | analyzed |
| `0x8004C77C` / `0x8004CA84` | `OptionsScreen_MonitorTuning` / `OptionsScreen_ResetItem` | analyzed/guessed |
| `0x800481C0` | `GameOverScreen_Update` (RETRY/CONTINUE) | analyzed |
| `0x800497C4` / `0x80047438` | `ResultsTime_Draw` / `ResultsTarget_Draw` (TARGET:%d) | analyzed/guessed |

## Overlays — RESOLVIDO (Fase 1)

Não há código de overlay no disco:

- Só existe 1 PS-X EXE no ISO (o boot, 630.784 bytes).
- `ZNULL.DAT` (32.256.000 bytes @ LBA 161.708) é o **dummy de padding da
  Capcom** (padrão RE2/Dino Crisis): seu extent cobre exatamente a região do
  track 2 (começa 150 setores após o fim do track 1 = pregap, termina no fim
  do track 2). É inacessível via FS e nunca lido — só preenche o anel do disco.
- Track 2 é áudio puro (sem sync pattern de setor — verificado).
- **Consequência:** todo o código do jogo está no EXE boot
  (`0x80018000..0x800B1800`), e é exatamente isso que o recomp cobre. O guardião
  `overlay_region_floor` do runtime é profilático.

## Leads para a Fase 2 (IA dos dinossauros / danger meter)

O gameplay detalhado vive na **sub-FSM** do GameTask (tabela em `0x800a8920`,
16 estados dumpados em Fase 1). Leads concretos:

- Struct de estado do gameplay: `0x800c7c40` (0xB0 bytes, "game context" do
  task) e work area `0x800c2574` (0x850 bytes, copiada p/ `0x800c2574`+0x9a98
  ao iniciar).
- Struct global "G": base `0x800B7A20` (lui em GameMain); frame counter em
  G+0x04; campo de jogo em G+0x28/0x2a/0x2c (state machine da tela); G+0x70/73
  = fade; G+0x6d/0x6f = flags de transição.
- Tabelas de dados: itens `0x800a593c` (stride 0xC); nomes de porta/evento
  (`D01PS`, `HDOR59`...) em `0x800A8E20+`; dicionário de texto comprimido
  (strings estranhas tipo `WBADTHEIRC` são pares de palavras — o texto do jogo
  é comprimido por dicionário).
- Dino AI provavelmente: handlers da sub-FSM + funções grandes de 0x8005xxxx–
  0x8006xxxx (movimento/ataque) — explorar callers de `EventFlag_*` e da
  struct G.
- Danger meter: procurar código que lê distância/julgamento do dino e escreve
  na cor do HUD (pulse vermelho) — sem âncora de string; requer traçado de
  runtime (`PSX_DEBUG_TOOLS`) ou varredura da sub-FSM.

## Ferramentas de análise criadas (`analysis/`)

| Script | Função |
|---|---|
| `decompile.py` | decompila funções passadas como args (`-postScript decompile.py 0x800...`) |
| `dump_states.py` | dumpa tabelas de estados + decompila primeiros handlers |
| `fix_labels.py` | renomeia FUN_ → func_XXXXXXXX (consistência com C gerado) |
| `apply_symbols.py` | aplica o bloco curado do symbols.toml no projeto Ghidra |
| `ghidra_import.py` / `verify_import.py` | script oficial do analyze / contagem de funções |
| `functions.tsv` / `analysis.json` / `edges.json` / `refs.json` | banco da análise estática |

Demais 2.151 funções: nomes `func_XXXXXXXX` no Ghidra (com plate comment de
confiança) + notas de assinatura no bloco append-only do `symbols.toml`.

## Fase 2 — decompilação casada (2026-10-10)

Critério, contrato e verificação: **[MATCHED.md](MATCHED.md)**. Toolchain
original: **[COMPILER.md](COMPILER.md)** (GCC Psy-Q, MIPS I, build
15/07/1999).

Primeiras funções substituídas (área event flags — o "micro-kernel" de
eventos do jogo, chamado por todo o gameplay):

| Segmento | Nome | Verificação |
|---|---|---|
| `0x8004A87C` | `EventFlag_Get` | 30.000 casos fuzzados, 0 divergências |
| `0x8004A7E8` | `EventFlag_Set` (entrada/clear) | idem (após correção de delay slot pega PELO teste) |
| `0x8004A830` | `EventFlag_Set_Store` (set + caso especial group1/bit2) | idem |

Fonte legível: `src_game/event_flags.c`. Remoção dos originais dos shards:
`tools/filter_generated.py` (configure-time) → `build/gen-src/`. Gate de
regressão: alvo `psx-matched-test` (roda no build do teste). Semântica
documentada: flag table em `0x8009CD18+group*8`, evento group1/bit2 reseta
física (`0x800B58B8`, `0x800B5898`) e timers (`0x1F801108/A`).

Próximos candidatos (MATCHED.md): `Task_Clear`, `Task_SetStart`, depois as
demais funções da área core.
