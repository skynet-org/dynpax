---
name: dynpax-runtime-debug
description: "Debug a Dynpax chroot, Docker, loader, or missing-library failure with the runtime specialist."
argument-hint: "Describe the runtime failure or reproduction"
agent: dynpax-runtime-debugger
---

Debug this Dynpax runtime problem:

${input}

Instructions:

- Reproduce the exact failing command first when a reproduction is available.
- Identify whether the failure is about dependency discovery, planned bundle
  paths, materialized layout, interpreter placement, rewritten RUNPATH, or
  alias-chain resolution.
- Use the smallest evidence set that discriminates those causes.
- Re-run the same reproduction after the fix before widening scope.

Close with:

- the reproduction used
- the concrete runtime cause found
- the validation rerun after the fix
- any remaining gap such as missing musl coverage
