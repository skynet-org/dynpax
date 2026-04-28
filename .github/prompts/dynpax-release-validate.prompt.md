---
name: dynpax-release-validate
description: "Review or change Dynpax CI, semantic-release, or GHCR release automation with the release specialist."
argument-hint: "Describe the release or workflow task"
agent: dynpax-release-validator
---

Work on this Dynpax release-automation task:

${input}

Instructions:

- Start from the exact workflow file, trigger path, or release-automation edge
  that controls the behavior.
- Check token provenance, workflow permissions, trigger conditions, image tags,
  and architecture-specific paths before proposing changes.
- Preserve the repo's requirement that downstream release workflows continue to
  fire when tags or releases are created.

Close with:

- the workflow path affected
- the risk or change identified
- the validation performed
- any remaining risk that cannot be executed locally
