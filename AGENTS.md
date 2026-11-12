## Environment

- Machine-specific facts — which extensions are encrypted containers, which
  processes may read or write them, console encoding, tool install paths — belong
  to the workstation, not to this repo: the global agent instructions carry them.
  This file does not restate them, so it stays true on a machine without the
  encryption client.
- Read sources with `python tools/read_source.py <path>`.
- Building, regenerating the config snapshot or flashing misbehaves: read
  `docs/agents/environment.md` first.

## Agent skills

### Issue tracker

Issues and specs live as GitHub issues in `zhengjy926/nihao`, managed with the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

The five canonical triage roles use their default label strings (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
