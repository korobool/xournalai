# How the fork uses git (since 11 Nov 2026, S15.26)

The owner (11 Nov 2026): «хорошо затегай и зафиксируй текущую версию в Git репозитории, чтобы не возникла ситуация,
что нам сложно откатиться или трудно какие-то фичи разделить … не смешать все в один котел».

## Branches

| Branch | What it is |
|---|---|
| `mcp` | the fork's main line: every finished feature, every release |
| `master` | the same commit as `mcp` after each release (moved with `git branch -f master mcp`) |
| `ai/<feature>` | one feature while it is built, branched off `mcp` |

- One commit per task, as before: the message ends with the task id, `(T15.26.1)`. `tools/monitor/track.py` finds the
  task's commit by that suffix.
- A finished feature comes into `mcp` with **one merge commit** (`git merge --no-ff ai/<feature>`). Its message names
  the branch and the stage, **without** a task suffix, e.g. `ai: merge ai/record-page (S15.26)`.
- The release commit (version, changelog) is made on `mcp` after the merges and tagged `ai-vX.Y.Z`. Then `master`
  is moved to it, and `mcp`, `master`, the tag and the feature branches are pushed to the fork.
- Two features never share a branch. An experimental feature gets a release of its own, and its option is off by
  default, so it is separate at run time too.

## Going back

- **To a version:** every release has a tag. `git switch --detach ai-v1.21.0`, build and install as usual
  (`cmake --build build && cmake --install build`), then `git switch mcp` to come back.
- **Without one feature:** `git log --first-parent --merges mcp` lists the merged features, one line each.
  `git revert -m 1 <merge>` takes one of them out as a new commit; nothing else changes. Reverting that revert
  brings it back.
- **A list of what a feature changed:** `git diff <merge>^1 <merge>`, or `git log <merge>^1..<merge>^2` for its task
  commits.

## Rollback points

| Tag | Commit | What it is |
|---|---|---|
| `ai-v1.21.0` | `7fcfbbdcd` | before S15.26: one speech recognition for every tool (the last release made one commit at a time) |
| `ai-v1.20.0` | `60877d18b` | the Record menu with two questions; a shorter row |
| `ai-v1.18.0` | `ee5840e93` | the first research browser |
| `ai-v1.17.5` | `1609e90b2` | before the research browser |

Every earlier release has its tag too (`git tag -l 'ai-v*'`).
