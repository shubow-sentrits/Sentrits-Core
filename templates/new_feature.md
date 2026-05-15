---
name: implement-feature
description: Implement a narrowly scoped feature with minimal architecture disruption. Use when the user wants to add or change product behavior.
---

Implement {{FEATURE}} as the smallest useful vertical slice.

First find the existing code path that is closest to this behavior.

Change as little surface area as possible.

Do not redesign unless the current design directly blocks the feature.

Before coding, state the files/modules you will touch and the main risk.

Deliver: plan → patch → tests → residual risks.