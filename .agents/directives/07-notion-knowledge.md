# 07. Notion Knowledge Directive for AI Agents

This directive governs how AI agents use connected Notion documents as live project knowledge without copying them into this repository.

## Live Notion Sources

Use these Notion documents as project-specific live references:

- Game design notes / planning notes (`기획 정리`): `https://equinox-comet-f04.notion.site/37243b7906dd80ab994ace8cd9be7970`

## When To Consult Notion

Fetch the relevant Notion source before making decisions that depend on:

- Project concept, game design, player roles, or gameplay goals.
- Hugh and Diana role separation.
- Narrative, setting, terminology, or feature intent.
- Planning assumptions that are not already captured in repository docs.
- Any user request that mentions Notion, `기획`, design notes, planning notes, or project-specific documentation.

## Usage Rules

- Treat Notion as a live source: fetch the relevant page when needed instead of copying its content into the repo.
- Treat Notion as read-only unless the user explicitly asks to create, update, move, or delete Notion content.
- Prefer direct page URLs from this file over broad workspace search.
- If the Notion connector fails, try the CLI fallback below before giving up.
- If the user provides a newer Notion page or database URL, prefer the newer URL for that task and suggest updating this directive.
- When a decision is materially based on Notion, mention the Notion page title or URL used.

## CLI Fallback for Public Notion Pages

When Notion MCP/app tools cannot read a direct Notion page URL, treat the problem as a connector issue first, not proof that the page is unreadable.

Use this read-only CLI fallback for public Notion pages:

1. Check the URL with `curl.exe -L -I <url>` or PowerShell `Invoke-WebRequest`.
2. If the page returns `200 OK` but the HTML title is only `Notion`, call Notion's web client page loader directly.
3. Convert the page ID to dashed UUID form.
4. POST to `https://www.notion.so/api/v3/loadPageChunk` with this JSON shape:

```json
{
  "pageId": "<dashed-page-id>",
  "limit": 200,
  "cursor": { "stack": [] },
  "chunkNumber": 0,
  "verticalColumns": false
}
```

Parse `recordMap.block`; each entry's block payload is under `value.value`. Start from the root page block, read `properties.title`, then recursively follow `content` child IDs.

Only use this fallback for read-only access. Do not commit full Notion page dumps, generated exports, connector cookies, or local scratch files. If the direct web call returns `object_not_found`, an auth challenge, or no page blocks, report that the page is not accessible from the current environment and ask the user to fix sharing or connector workspace access.

## Naming Note

For English labels, translate `기획 정리` as `game design notes` when the context is gameplay or feature design, and as `planning notes` for broader project planning.
