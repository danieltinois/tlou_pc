# macOS (Apple Silicon)

[Português](#português) · [English](#english)

## Português

**Estado:** em desenvolvimento no fork `danieltinois/tlou_pc`, branch `feat/macos`. **Nenhum código do jogo foi executado ainda** (falta testar com o dump). Comprovado sem o jogo, sob o Rosetta 2:

- `build.sh --test`: o emulador de instruções, os stubs reescritos executando código x86 de verdade (desvio por `int3` no handler de sinal) e a proteção de red zone (falha de escrita → handler → reexecução) passam, assim como o heap baixo;
- janela, view Metal, device e swapchain Retina; com `TLG_BACKGROUND_WARMUP=0`, o presenter apresenta frames: o MoltenVK registrou ~480 apresentações em 8 s a 60 Hz, 16 shaders SPIR-V→MSL e 11 pipelines **do próprio port** (overlay, pós-processamento), sem erros.

Ainda **não** comprovado: carregar e executar o eboot, e qualquer shader **do jogo** (GCN traduzido pelo shadPS4) no Metal.

### Como funciona

O port roda o código x86-64 do `eboot.bin` dentro do próprio processo. Num Mac com Apple Silicon, o processo inteiro é **x86_64 e roda sob o Rosetta 2**; a GPU é acessada por Vulkan → MoltenVK → Metal. As diferenças do macOS ficam em:

| Assunto | Onde |
|---|---|
| Dependências x86_64 (SDL3, FFmpeg, Vulkan loader, MoltenVK, …) | `port/scripts/macos_deps.sh`, `port/macos-env.sh` |
| APIs de thread, locks com timeout, ucontext, diagnósticos via Mach | `port/src/compat_mac.h`, `port/src/ucontext_regs.h` |
| TLS do jogo em `gs:[8*chave]` (chave de pthread) | `port/src/runtime_thread.c` |
| Faixas de endereço do PS4 reservadas por segmentos do executável; o Rosetta ocupa `0xfc0000000`–`0x7000000000`, então a área do usuário começa em `0x7000000000` | `port/src/runtime_memory.c`, `port/gpu/CMakeLists.txt` |
| Objetos do host que o jogo enxerga ficam abaixo de 1 TiB | `runtime_guest_calloc` (`port/src/runtime.h`) |
| Janela na thread principal (exigência do Cocoa), superfície `CAMetalLayer` | `port/src/probe.c`, `port/gpu/shim/window.cpp`, `tlggpu.cpp` |
| Recursos nulos no lugar de `nullDescriptor` (o MoltenVK não tem) | `vk_instance.cpp`, `vk_rasterizer.cpp` |

### Compilar

Requisitos: macOS 15 ou mais novo (AVX no Rosetta), Rosetta 2 (`softwareupdate --install-rosetta`), Xcode Command Line Tools e Homebrew.

```bash
brew install cmake ninja pkgconf nasm glslang boost
cd port
bash scripts/macos_deps.sh      # uma vez: bibliotecas x86_64 em out/macos-deps/prefix
bash build.sh --test            # out/tlg-probe (x86_64) e o teste do heap baixo
```

### Arquivos do jogo

Os mesmos da [lista de arquivos necessários](ARQUIVOS_NECESSARIOS.md) (CUSA00552, jogo base 1.00 + update 1.11), numa pasta só, em **`game/CUSA00552/`** na raiz do repositório (a pasta `game/` é ignorada pelo git):

```
tlou_pc/game/CUSA00552/
  eboot.bin                 (update 1.11)
  sce_module/libc.prx, sce_module/libSceFios2.prx
  patch.psarc, data.psarc, build/ps4/, sce_sys/, sce_discmap*.plt
```

O pendrive exFAT do Itemzflow monta sozinho em `/Volumes/<nome>`. Para juntar jogo base e update sem duplicar ~56 GB, com links:

```bash
python3 scripts/make_game_view.py /Volumes/<nome>/CUSA00552 /Volumes/<nome>/CUSA00552-patch game/CUSA00552
```

Outra pasta: `TLOU_GAME_DIR=/caminho ./run_tlou.sh`.

### Rodar

```bash
./run_tlou.sh
```

O `tlou_run.py` define `ROSETTA_ADVERTISE_AVX=1` (o Rosetta só anuncia AVX com ela) e usa o MoltenVK que o `build.sh` copia para `port/out/`. O log fica em `port/out/logs/`.

### A observar quando o jogo rodar

- O MoltenVK não tem `robustBufferAccess2`, `nullDescriptor`, geometry shaders, `shaderFloat64`, `logicOp` nem `samplerMipLodBias`; vários formatos de textura do GCN não têm suporte completo no Metal (avisos no log).
- A imagem nula é uma view 2D RGBA8: slots 3D, cubo ou de formato inteiro sem recurso podem dar artefatos.
- Locks com timeout usam trylock + espera curta; endereços fixos do jogo dentro da faixa do Rosetta não podem ser atendidos.
- `exit()` com threads da GPU vivas aborta no macOS ("mutex lock failed"); use `_exit`.
- No macOS, acesso a página mapeada mas protegida chega como **SIGBUS** (no Linux, SIGSEGV); só endereço não mapeado é SIGSEGV. O `probe.c` encaminha os dois ao rastreamento de páginas da GPU no Mac.

## English

**Status:** in development in the `danieltinois/tlou_pc` fork, branch `feat/macos`. **No game code has run yet** (needs the dump). Proven without the game under Rosetta 2: `build.sh --test` runs real x86 code (rewritten stubs, int3 and write-fault handling, which arrives as SIGBUS on macOS) and the low heap; the window, device and Retina swapchain come up, and with `TLG_BACKGROUND_WARMUP=0` MoltenVK logged ~480 presents in 8 s at 60 Hz, 16 SPIR-V→MSL shaders and 11 pipelines of the port's own (overlay, post-processing), without errors. Not proven: loading and running the eboot, and any game (GCN) shader on Metal.

The port runs the eboot's x86-64 code in-process, so on Apple Silicon the whole process is **x86_64 under Rosetta 2**, with Vulkan → MoltenVK → Metal for graphics. See the table above for where each macOS difference lives.

**Build:** macOS 15+, Rosetta 2, Xcode Command Line Tools, Homebrew; then `brew install cmake ninja pkgconf nasm glslang boost`, and in `port/`: `bash scripts/macos_deps.sh` (once) and `bash build.sh --test`.

**Game files:** the [required files](ARQUIVOS_NECESSARIOS.md#required-game-files) (CUSA00552, base 1.00 + update 1.11) merged in **`game/CUSA00552/`** at the repository root (ignored by git), e.g. with `scripts/make_game_view.py` from the exFAT drive under `/Volumes/<name>`; or set `TLOU_GAME_DIR`.

**Run:** `./run_tlou.sh` (sets `ROSETTA_ADVERTISE_AVX=1`; MoltenVK sits next to `port/out/tlg-probe`). Logs in `port/out/logs/`.
