---
name: dynpax-route-task
description: "Route a Dynpax task to the right specialist agent for layout work, runtime debugging, or release validation."
argument-hint: "Describe the Dynpax task to route"
agent: "agent"
tools: [agent, read, search]
---

Route this Dynpax task to the most appropriate specialist agent.

Task:

${input}

Routing rules:

1. Use `dynpax-layout-specialist` for:
   - FlatLib64 or PreserveSourceTree changes
   - bundle path planning
   - FakeRoot layout
   - compatibility symlinks
   - bundled `RUNPATH` expectations
   - layout-driven verifier updates
2. Use `dynpax-runtime-debugger` for:
   - `chroot` failures
   - missing shared libraries at runtime
   - broken SONAME alias chains
   - Docker matrix regressions
   - preserve-source-tree runtime failures
   - real-binary reproductions such as `/usr/bin/openssl`
3. Use `dynpax-release-validator` for:
   - GitHub Actions workflow changes
   - semantic-release and release-tag automation
   - GHCR publishing
   - multi-arch image manifests
   - release tokens, permissions, or trigger flow

Instructions:

- Choose exactly one specialist agent when the task clearly matches one.
- If the task is too small or does not match any specialist, handle it in the
  current agent and say why no specialist handoff was needed.
- Before handing off, give one short sentence explaining the routing decision.
- After the specialist responds, summarize the outcome in concise project terms.
