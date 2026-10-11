# TEXTURES — Pipeline de AI upscaling (Nível 3)

Objetivo: melhorar o visual sem refazer assets — upscaling AI das texturas
originais do jogo, com destino ao pack de texturas HD Beetle-style que o
runtime do PSXRecomp já embute (`hd_texture_pack.cpp`).

## Estado atual (2026-10-10)

| Etapa | Status |
|---|---|
| Upscaler AI instalado na RTX 4060 | ✅ Real-CUGAN (+ waifu2x alternativo) |
| Hook de dump de uploads no runtime | ✅ `PSX_TEX_DUMP_DIR` (gpu.c) |
| Inventário de texturas | ✅ 10.208 texturas únicas (`inventory.tsv`) |
| 20 primeiros assets upscaleados | ✅ `pack/original` → `pack/upscaled` (4×) |
| Lookup do pack no caminho de draw (substituição real) | ⏳ próximo passo |
| Nomes finais `<texhash>-<palhash>.png` | ⏳ exige o lookup (palhash é do draw) |

## 1. Upscaler instalado

Ferramentas em `~/Tools/` (fora do repo), todas via Vulkan na RTX 4060
(**sempre passar `-g 0`** — o auto-select cai no llvmpipe/CPU):

- **Real-CUGAN** (primário, ideal para arte de PS1): `~/Tools/realcugan/realcugan-ncnn-vulkan-20220728-ubuntu/realcugan-ncnn-vulkan`
  - `-m models-se -n <0|1|2|3> -s <2|3> -g 0` (noise 0 preserva o dithering autêntico; modelos `-pro`/`-nose` também disponíveis)
- **waifu2x** (alternativo): `~/Tools/waifu2x/waifu2x-ncnn-vulkan-20220728-ubuntu/waifu2x-ncnn-vulkan -n cunet -s 2 -g 0`
- Real-ESRGAN ncnn v0.2.0: **quebrado** (segfault com qualquer modelo externo — descartado).

Comando do lote usado:

```bash
RC=~/Tools/realcugan/realcugan-ncnn-vulkan-20220728-ubuntu/realcugan-ncnn-vulkan
for f in assets/textures/pack/original/*.png; do
  n=$(basename "$f" .png)
  $RC -i "$f" -o "assets/textures/pack/upscaled/${n}_4x.png" \
      -m ~/Tools/realcugan/realcugan-ncnn-vulkan-20220728-ubuntu/models-se \
      -n 0 -s 4 -g 0
done
```

## 2. Como as texturas foram extraídas

O jogo **não tem TIMs** — nem no EXE (`disc/SLUS_009.22`, 0 encontrados) nem no
disco inteiro (scan dos 161.558 setores do Track 1: 0). Dino Crisis usa formatos
próprios comprimidos. Portanto a extração é **em runtime**: injetei um hook em
`psxrecomp/runtime/src/gpu.c` (`gp0_commit_cpu_to_vram`) que, quando a env
`PSX_TEX_DUMP_DIR` aponta para um diretório, salva cada upload CPU→VRAM como PNG
(`<texhash>_<w>x<h>.png`, deduplicado) e anexa uma linha em `dump.log`:

```
frame x y w h texhash
```

`texhash` usa a **mesma CRC-32** do pack Beetle-style (`hd_texture_crc32_words_le`),
então cada dump já tem a metade "textura" da chave final de substituição.
A outra metade (`palhash`, hash da CLUT viva no momento do draw) só existe no
caminho de desenho — ver §5.

Fluxo da sessão de captura: jogo com `PSX_TEX_DUMP_DIR` + debug server (porta
4370, `PSX_DEBUG_TOOLS=ON`) dirigido por injeção de input (`press`) cobrindo
boot, título, briefing, salas iniciais, inventário e pausa.

## 3. Inventário (agregado em `assets/textures/inventory.tsv`)

- **68.432 uploads** → **10.208 texturas únicas** (11 dimensões distintas).
- `320x240`: **26** — telas de boot/título/pause e composições fullscreen (16bpp, dumps limpos).
- `24x160`: **9.372** — páginas de **texto renderizado** (strings). NÃO são
  assets de upscaling; o alvo correto no futuro é o atlas de fonte.
- `32x32`: **782** — sprites/tiles (4/8bpp indexadas: precisam da paleta do draw, §5).
- Demais: faixas de UI (256x1..15, 32x8/16).

## 4. Os 20 primeiros assets (lote 1 — começo do jogo)

Originais em `assets/textures/pack/original/`, 4× em `pack/upscaled/`
(não versionados: arte do jogo — ver `.gitignore`; regenerar com §1+§2).

| # | asset | fonte | dims orig→4× | obs |
|---|---|---|---|---|
| 1 | boot_capcom_logo | dump 41342d41 | 320×240→1280×960 | 16bpp limpo |
| 2 | boot_warning_screen | dump b7b9f453 | 320×240→1280×960 | 16bpp limpo |
| 3 | title_screen_main | dump 4ffffc93 | 320×240→1280×960 | 16bpp limpo |
| 4 | title_screen_alt | dump 527683e1 | 320×240→1280×960 | 16bpp limpo |
| 5 | ui_item_screen_item | screenshot canônico | 320×480→1280×1920 | tela de itens (menu ITEM) |
| 6 | ui_item_screen_equip | screenshot canônico | 320×480→1280×1920 | tela de itens (menu EQUIP) |
| 7 | ui_equip_screen | screenshot canônico | 320×480→1280×1920 | EQUIPPED WEAPON/AMMO |
| 8 | ui_map_screen | screenshot canônico | 320×240→1280×960 | **capturou gameplay** — refazer (abrir MAP) |
| 9 | ui_operation_room | screenshot canônico | 320×240→1280×960 | OPERATION ROOM |
| 10 | ui_briefing_kirk | screenshot canônico | 320×240→1280×960 | briefing Dr. Kirk |
| 11 | bg_corridor_1f | screenshot canônico | 320×240→1280×960 | fundo 1º corredor (personagens queimados) |
| 12 | bg_office | screenshot canônico | 320×240→1280×960 | fundo sala inicial (Regina queimada) |
| 13 | bg_facility_yard | screenshot canônico | 320×240→1280×960 | pátio (Regina queimada, pequena) |
| 14 | ui_pause_overlay | screenshot canônico | 320×240→1280×960 | PAUSE sobre sala |
| 15–19 | ui_menu_item/equip/map/mix/exit | crop de #6 | 62×44→248×176 | botões do menu (bleed de vizinhos nas bordas) |
| 20 | char_regina_portrait | crop de #6 | 68×122→272×488 | retrato da Regina + label |

Notas: (a) fundos com personagem queimado servem como referência/validação
visual — a extração limpa de fundos depende de capturar no fade de sala ou do
caminho de draw; (b) `ui_menu_map` tem bleed inferior (EXIT invadindo) — refinar
crop; (c) `ui_map_screen` precisa ser recapturado com o mapa aberto de verdade.

## 5. Próximos passos (continuação em lote)

1. **Wiring do pack no draw** (runtime): em `gpu_gl_renderer.c`, no ponto onde a
   primitiva consulta a text page, chamar `hd_texture_pack_lookup(texhash_da_residência,
   hd_texture_hash_clut(vram, clut_x, clut_y, depth))` e usar o PNG decodificado.
   Toda a maquinaria (`hd_texture_pack.cpp`, já compilada no runtime) existe;
   falta o consumidor. O mesmo ponto deve, em modo dump, logar pares
   `(texhash, palhash)` — o join com `dump.log` gera os nomes finais
   `<texhash>-<palhash>.png` e habilita a substituição real.
2. **Texturas indexadas (4/8bpp)** — personagens (Regina, dinos), ícones de item,
   HUD: no dump atual elas saem como palavras crus (ruído visual). Com o passo 1,
   o dump no draw sai com a paleta correta e vira asset upscaleável.
3. **Fundos sem personagem**: capturar no fade de transição de sala, ou via
   MDEC/upload path do draw.
4. **Lote 2+**: rodar novas sessões com `PSX_TEX_DUMP_DIR` cobrindo mais salas/
   menus, priorizar por `inventory.tsv` (área × frequência de upload), repetir §1.
5. **Ativação**: criar `dc.texture-hd` no `state.toml`/launcher quando o passo 1
   estiver no ar (env `PSXRECOMP_HD_TEXTURE_ROOT` ou root explícito no
   `hd_texture_pack_create`).

## Arquivos

- `tools/tim_extract.py` — extrator/validador TIM (scan de EXE/disco; provou 0 TIMs)
- `tools/texture_inventory.py` — consolida `dump.log` → `inventory.tsv`
- `tools/patches/psxrecomp_texture_dump.patch` — o hook `texture_dump_upload` em `psxrecomp/runtime/src/gpu.c` (o psxrecomp é submódulo upstream; aplicar após o clone com `git -C psxrecomp apply ../tools/patches/psxrecomp_texture_dump.patch` e reconfigurar o cmake)
- `assets/textures/runtime-dumps/` — 10.030 PNGs + dump.log (local, não versionado)
- `assets/textures/pack/` — original/ e upscaled/ (local, não versionado)
