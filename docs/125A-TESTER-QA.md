# 125A Plugin Tester CI integration – 2026-10-12

The current branch defines an isolated release-QA phase using the exact
125A-Plugin-Tester-Final v0.2.9 source SHA
\`fd246146994384e301c735e7d860b53aa06b18c5\`.
It builds and runs the production dispatcher, full worker,
reload/instantiation, fresh-state, I/O/event, editor-lifecycle and Steinberg
Validator probes on the **same Pitchator VST3 bundle** that passed
the regular native Steinberg Validator. The job archives all 125A text
and JSON diagnostics even on a failure.

**QA pass must not be claimed until this workflow actually succeeds**.

This test is designed to catch measurable host-independent integration defects
before any Studio One handoff. It does not simulate human mouse interactions
inside Studio One and does not waive the 125A requirement for real 100/150%
zoom/hitbox/automation and DAW-routing acceptance.

After the bounded test run, CI returns to workflow_dispatch only.
