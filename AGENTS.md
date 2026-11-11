## Environment

- Transparent file encryption on this workstation: `.c`, `.s`, `.dts` and
  `.uvprojx` files are ciphertext containers on disk. `python`, `cmd /c type`,
  `ripgrep`, `git` and the Keil toolchain (armclang/armasm/armlink/UV4) read
  plaintext; PowerShell/.NET (`Get-Content`, `Select-String`, `[IO.File]`) and
  the VS Code host read `%TSD-Header-###%`.
- Read sources with `python tools/read_source.py <path>`; never with
  `Get-Content`/`Select-String` (the latter returns 0 matches *silently* and
  corrupts conclusions).
- Build/config quirks (AC6 `-E`, venv Python, uVision batch builds, BOOT0):
  see `docs/agents/environment.md`.

## Agent skills

### Issue tracker

Issues and specs live as GitHub issues in `zhengjy926/nihao`, managed with the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

The five canonical triage roles use their default label strings (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
