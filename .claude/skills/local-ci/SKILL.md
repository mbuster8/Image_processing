---
name: local-ci
description: Run ID Vision Lab's CI pipeline on this machine before anything is pushed — clang-format 22.1.8, the debug and AddressSanitizer builds and tests, and the Ubuntu 24.04 job (OpenCV 4.6, Qt 6.4) in Docker — and report each step's result with the cause of any failure. Use this whenever the user wants changes checked before a push or pull request, asks whether something passes CI, wants the tests run on Linux, Ubuntu or in Docker, wants a CI failure reproduced locally, or says "run the pipeline", "verify locally" or "does it build on Ubuntu", even without naming the script.
---

# Local CI

`scripts/check.sh` runs what `.github/workflows/ci.yml` runs, so a push meets no surprises. CI has three jobs:
macOS (debug and asan), Ubuntu 24.04 (debug, and the build without Qt) and clang-format. The script runs the
first and third natively and the Ubuntu job in a container built from `docker/ubuntu-24.04.Dockerfile`. CodeQL
is not reproduced locally.

| Command | What it runs | Typical time |
|---|---|---|
| `scripts/check.sh` | clang-format, debug, asan, Ubuntu in Docker | a few minutes; the first Docker build longer |
| `scripts/check.sh --quick` | clang-format, debug | about a minute |
| `scripts/check.sh --linux` | the Ubuntu job only | 2–4 minutes |

## Choosing the mode

- Before a push or a pull request, or after changing C++, CMake, the presets or `.github/`: the full run. CI
  failures so far have come from Linux only (a locale) and from load only (a race), which a quick run misses.
- While iterating on one change: `--quick`, then the full run before handing the work back.
- Reproducing a failure seen only on CI's Ubuntu job: `--linux`.
- A change to Markdown only needs no run; say so rather than spending minutes on it.

## Running it

Run it from the repository root, in the background, with a 10-minute timeout: it prints one line per step and
nothing else, so wait for it to finish rather than polling. The output ends with `check.sh: all steps passed.`
or names the failed step; never report a pass without that line.

If Docker is not running, the script says so. Ask the owner to start Docker Desktop, or offer `--quick` and
say the Ubuntu job was not run. Do not start Docker yourself unasked.

## When a step fails

1. Read the step's full log, `build/check-logs/{format,debug,asan,ubuntu}.log` (the script prints its path and
   last 40 lines).
   Find the first real error: a compiler `error:`, a test's `Failure` block, or `[  FAILED  ]` lines.
2. Reproduce the one failing test directly, faster than rerunning everything:
   - here: `build/debug/tests/<binary> --gtest_filter=<Suite.Test>` (or `build/asan/...`);
   - on Ubuntu: `docker run --rm -it --volume "$PWD":/src:ro --entrypoint bash id-vision-lab-ubuntu`, then run
     `/src/docker/run-ubuntu-tests.sh` or the single test inside it.
3. A test that fails on CI but passes here may need load to show: repeat it in several processes at once
   (`--gtest_repeat=200`, 16 processes). That is how a port race in `Server::stop()` was reproduced.
4. Find the root cause before fixing (the systematic-debugging skill); fix with a test that fails first.

## Reporting

Give a short table — step, result, time — then, for a failure, the failing test or file, its first error line,
the cause if known, and the log path. Mention steps that were skipped (Docker down, `--quick`).

## The pre-push hook

`scripts/git-hooks/pre-push` runs the full script before every `git push`. It is on only when the clone has
`git config core.hooksPath scripts/git-hooks`; suggest it, but change the owner's git configuration only when
asked. `git push --no-verify` skips it once.

## Limits

- The working tree is checked, uncommitted changes included; the script says when there are some.
- Windows is not covered yet: the Windows port brings its own presets and CI job.
- Never push, and never commit unless the owner asked for it in that request.
