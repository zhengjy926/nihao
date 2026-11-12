## Environment

- Machine facts (transparent encryption, GBK console, tool install paths) live in
  the workstation's global instructions; `docs/agents/environment.md` maps them
  onto this repo's tooling. Never restate one machine's state as a rule for all.
- Search with `python tools\find_in_sources.py <regex> [paths...]`: it exits 3,
  not 1, while files are still `%TSD-Header-###%` containers, so "no match" stays
  a fact.
- Building, regenerating the config snapshot or flashing misbehaves: read
  `docs/agents/environment.md` first.

## Agent skills

### Issue tracker

Issues and specs live as GitHub issues in `zhengjy926/nihao`, managed with the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

The five canonical triage roles use their default label strings (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
