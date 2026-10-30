---
name: pr-ready
description: Get an ID Vision Lab branch ready for a pull request — run the full local CI, review the branch's changes against its base, deal with what the review finds, and draft the pull request title and description with the test evidence. Use this whenever the user asks to review the branch or the current set of changes before a PR, to prepare or write a PR description, to check whether a branch is ready to push or merge, or says "get this ready for review", even if they mention neither CI nor review.
---

# Ready for a pull request

The owner reviews every change and opens pull requests themselves. This skill hands them a branch that passes
the same checks CI will run, a review of its changes, and a description they can paste. It never pushes, opens
the pull request, or commits unless the owner asks for that in the request.

## 1. Establish what is being proposed

- `git status` and the branch name. Uncommitted changes are not part of a pull request: say so, and ask
  whether they belong in it before going on.
- The base: usually `master`. A branch built on another unmerged branch (A1 on A0, for example) is reviewed
  against that branch; ask when it is not clear. Then `git log --oneline <base>..HEAD` and
  `git diff --stat <base>...HEAD`.

## 2. Run the local CI

Use the `local-ci` skill in its full mode. If a step fails, stop here: report it, find the cause, and propose
the fix. A description of a branch that fails CI would be wrong the moment it is pasted.

## 3. Review the changes

Use the `code-review` skill on the branch against its base, at `medium` effort unless the owner names another.
Report its findings ranked. Fix the clear ones only if the owner agrees (or asked for fixes up front); after
any fix, rerun the local CI's affected steps.

Check the project's own rules too, which a generic review may not know (`CLAUDE.md`): the libraries stay
Qt-free; wire-format changes update `docs/wire-format.md` and `docs/user-guide.md` together; zero warnings;
British English; nothing committed links into `docs/superpowers/`; tests never use port 7139.

## 4. Draft the description

Show it in the conversation, ready to paste; write no file for it.

```
<Title: what the branch does, in the style of its commits, under 70 characters>

## Summary
<two to four bullets: what changes for a user or a developer, and why>

## Changes
<per commit or per area, one line each>

## How it was tested
| Check | Result |
|---|---|
| clang-format 22.1.8 | passed |
| debug (macOS), <n> tests | passed, <time> |
| asan (macOS), <n> tests | passed, <time> |
| Ubuntu 24.04 in Docker, <n> + <n> tests | passed, <time> |
<manual checks done, with what was looked at>

## Notes for the reviewer
<decisions, risks, follow-ups; omit when there are none>

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

Take every number from this run's output; leave a row out rather than guess it. Write in British English, and
link only to files that are committed.

## 5. Hand over

Say what passed, what the review found and what was done about it, and that the branch is ready (or not) for
the owner to push and open. `gh` may not be installed; the owner opens the pull request in the browser.
