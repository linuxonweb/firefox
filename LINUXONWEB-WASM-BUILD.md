# Firefox 159.0a1 for the linuxonweb wasm guest

This repository is Firefox 159.0a1 (Nightly) source, built as a 32-bit wasm
module with the musl C library, for the linuxonweb browser guest. It has two
commits:

1. **Upstream snapshot.** `bf9757d91e`, the Git mirror of Mozilla's
   mozilla-central changeset `dfc77f7a6d716f1a20a2576597638d4fc21cd916`, the
   changeset the build records. Nothing else is changed in this commit.
2. **linuxonweb changes.** The net effect of everything the wasm build does
   to the tree: about 105 modified files and 13 new ones (adapters, generated
   static-name headers, the PBL JIT source and `defs.mk`). `git diff HEAD~1`
   shows exactly what was changed.

`linuxonweb-build/` holds the scripts, patches and evidence that produced
those changes. It is the same as `tools/nightly-musl/` in the linuxonweb
project.

## Status

- The build compiles all 3,012 expected native-wasm object paths.
- The NSS `certutil` binary links and validates (`wasm-validate`).
- A complete linked Firefox browser has **not** been produced in this
  repository. Guest startup, JIT, TLS and GPU acceptance are not established.
  See `linuxonweb-build/recipe/` and `linuxonweb-build/evidence/` for the
  dated run records.

## How the build works

The build runs from a fixed set of Nix store paths (wasm32-musl LLVM 22.1.8,
binaryen 129, rustc nightly 2026-07-22, Python 3.11, Node 24). They are
listed in `linuxonweb-build/recipe/environment.sh`. The toolchain is not
checked in.

1. **Configure.** `recipe/mozconfig` sets the target
   (`wasm32-unknown-linux-musl`), the browser application and the cairo
   GTK3/X11 toolkit. It disables the JIT, LTO, debug symbols and sandbox, and
   uses the Nightly update channel. `--disable-jit` is deliberate: the JIT is
   added later as a separate object (step 5).
2. **Port.** The tree is ported to the musl wasm target by the patches in
   `linuxonweb-build/patches/`, each with a `*-port.py` script in
   `linuxonweb-build/recipe/` that applies it. The recurring changes are:
   - wasm32 musl adapters for pthread, mmap, time64 and the libc ABI
     (`native-tagged-mmap`, `native-libevent-time64`, `native-abseil-*`,
     `native-llama-mapping`);
   - native-only fallbacks for APIs musl lacks, such as `fork` and debugger
     backtraces (`no-fork-error-paths`, `ggml-backtrace`), which never change
     the Linux path;
   - Rust crate fixes for the wasm32 libc and Vulkan/WebGPU backends
     (`rustix-wasm32-libc-only`, `wgpu-core-native-backend-cfg`,
     `wgpu-bindings-native-interop`).
   The tree in this repository already has these patches applied. The patch
   files record what changed. `recipe/run-phase.py` replays the phases:
   `configure`, `pre-export`, `export`, `mfbt`, `build`.
3. **Compile.** `mach build` compiles the C, C++ and Rust code into
   `obj-nightly/` (not in this repository). Each phase has a time limit, and
   the build keeps a fixed 20 GiB disk allowance. Full builds use 2 to 4 jobs.
4. **Native object checks.** `recipe/object_goals.py` and
   `recipe/freeze-inttypes-*.py` check which of the 3,012 expected native
   objects exist and whether their dependencies are up to date.
5. **PBL JIT.** `linuxonweb-build/pbl-jit/` contains the Portable Baseline
   Interpreter JIT: `PortableBaselineInterpret.patch`, the inline-cache
   sources (`PblIc.inc`, `PblIcOps.h`), `PblMethod.inc`, and `compile-ic.sh`.
   It compiles `PortableBaselineInterpret.o` and replaces that member in
   `libjs_static.a`.
6. **Link.** `linuxonweb-build/release/firefox-link.args` is the `wasm-ld`
   argument list (no debug info), and `firefox-objects.list` names its
   inputs. `build-firefox-release.sh` also compiles the guest `glremote.o`
   GL bridge and links the result as `firefox.preoptimizer.wasm`. The
   `glremote` source is in the linuxonweb project
   (`web/linux/tests/guest-glremote/`), not in this repository.
7. **Optimize.** `wasm-opt -O2` produces `firefox.optimized.wasm`, and then
   `--fpcast-emu` (a function-pointer ABI pass, `max-func-params@23`)
   produces `firefox.fpcast.wasm`. Test relinks set `FAST_SKIP_O2=1` to skip
   the `-O2` step. That output is not a release build.
8. **Package.** `package-firefox-native-gpu.py` and `slim-firefox-package.py`
   build the boot package, which adds a CA-only NSS seed and the initramfs.
   `release-report.py` writes the build report. Only CA certificates are
   packaged, never private keys.

## Reproduce

```sh
# from the repository root, on Linux x86_64 with the Nix store paths above
python3 linuxonweb-build/recipe/run-phase.py configure RUN1
python3 linuxonweb-build/recipe/run-phase.py build RUN1
bash linuxonweb-build/release/build-firefox-release.sh /path/to/out
```

The release script assumes the linuxonweb project layout, with the Nightly
tree at `/mnt/p3/project/linuxonweb-build/...`. Edit the paths in
`linuxonweb-build/recipe/environment.sh` and `release/build-firefox-release.sh`
first. Build outputs go on persistent disk, never in `/tmp` or tmpfs.

## Provenance

- Upstream: `mozilla-firefox/firefox` (Git mirror),
  `bf9757d91e2e53b8ab836f2a08bf4c0275fdcd23`, equal to the mozilla-central
  changeset `dfc77f7a6d716f1a20a2576597638d4fc21cd916` (Firefox 159.0a1).
- Full provenance and proof files: `linuxonweb-build/evidence/`.
