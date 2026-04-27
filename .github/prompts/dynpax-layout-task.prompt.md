---
name: dynpax-layout-task
description: "Work on a Dynpax FlatLib64 or PreserveSourceTree task with the layout specialist."
argument-hint: "Describe the layout-policy task"
agent: dynpax-layout-specialist
---

Work on this Dynpax layout-policy task:

${input}

Instructions:

- Start from one concrete anchor such as a failing layout policy, bundle path,
  verifier report, or owning file.
- Trace the nearest controlling code before editing.
- Prefer the smallest grounded fix over a broad refactor.
- Run focused validation immediately after the first edit.
- If runtime layout changes, include Docker-matrix validation when it is the
  narrowest meaningful check.

Close with:

- the exact layout behavior changed
- the validation performed
- any remaining runtime risk
