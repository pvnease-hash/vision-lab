#!/usr/bin/env bash
# Makes `pio run` work in a fresh cloud container. Idempotent and quiet when
# everything is already in place.
#
# Four things need fixing, all caused by the agent proxy blocking
# api.registry.nm1.platformio.org (403 on CONNECT) while GitHub and PyPI are
# allowed. Details in README.md.
set -u
log() { printf '[bootstrap-pio] %s\n' "$1"; }

command -v pio >/dev/null 2>&1 || {
  log "installing platformio toolchain deps"
  # click must stay <8.2: esptool 5.0.0-dev1 calls the old
  # ParamType.get_metavar() signature and the image step dies otherwise.
  pip install --quiet platformio scons rich_click "click==8.1.8" || log "pip install failed"
}

# tool-scons lives only in the blocked registry, so satisfy PlatformIO from the
# PyPI copy of SCons instead.
D="$HOME/.platformio/packages/tool-scons"
if [ ! -f "$D/scons.py" ]; then
  log "shimming tool-scons from PyPI SCons"
  mkdir -p "$D"
  printf '{"name":"tool-scons","version":"4.41101.0","system":"all"}\n' > "$D/package.json"
  printf '{"type":"tool","name":"tool-scons","version":"4.41101.0","spec":{"owner":"platformio","id":null,"name":"tool-scons","requirements":null}}' > "$D/.piopm"
  printf 'import sys\nfrom SCons.Script import main\nif __name__ == "__main__":\n    main()\n' > "$D/scons.py"
fi

# The RISC-V toolchain unpacks to packages/riscv32-esp-elf/bin, but PlatformIO
# exports packages/toolchain-riscv32-esp, so the compiler is not on PATH.
if ! command -v riscv32-esp-elf-g++ >/dev/null 2>&1; then
  BIN="$HOME/.platformio/packages/riscv32-esp-elf/bin"
  if [ -d "$BIN" ]; then
    log "linking RISC-V toolchain onto PATH"
    ln -sf "$BIN"/riscv32-esp-elf-* /usr/local/bin/ 2>/dev/null || true
  else
    log "toolchain not downloaded yet - first 'pio run' fetches it, then re-run this hook"
  fi
fi
exit 0
