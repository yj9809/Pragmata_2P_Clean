# CLAUDE.md

This project uses [`AGENTS.md`](AGENTS.md) as the canonical agent manifest.

Read and follow `AGENTS.md`; this file is only a thin redirect for Claude Code.

Project-local skills are canonical under `.agents/skills/`. Claude Code discovers
the generated wrappers under `.claude/skills/`; refresh them after adding or
renaming a skill with:

```powershell
.\Tools\Claude\sync-skills.ps1
```
