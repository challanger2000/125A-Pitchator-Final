# 125A Pitchator V1 — Release Audit (in progress)

Last fully binary-tested baseline: branch `v1.0.0`, commit `67969079364847059cae77b31682a99a5218ec51`; Windows GitHub Actions run [38329739159](https://github.com/challanger2000/125A-Pitchator/actions/runs/38329739159). Since that run, audit-only documentation and test-coverage changes have been committed. The added tests are **NOT YET EXECUTED IN CI**; no production VST3 implementation or UI resource has changed after the successful run.

## Independently evidenced gates
- Windows x64 VST3 build: PASS (job successful).
- Nine CTests: 9/9 PASS (run logs).
- Steinberg validator: PASS (job passed).
- 125A Plugin Tester Final v0.2.9: PASS, including MIDI I/O probe and editor lifecycle (job passed).
- Branded prototype artifact upload: PASS (workflow succeeded).
- Human Studio One checks: user reports MIDI output, curve interaction, project save/load and separate 125A tester PASS in chat. These are valuable direct host reports, not CI-generated replay/automation traces.
- FINAL release: **NOT APPROVED** pending gate closure below.

## Control/parameter inventory
Every MIDI parameter: `100 Key`, `101 Scale`, `102 Length`, `103 Bend Range`, `104 Scale Lock`, `105 Trigger`, `106 Loop`, `107 Bypass`. Source: `src/PitchatorVST.h`, `src/PitchatorVST.cpp`. Declared UIDESC tags match these eight registrations. Default new Trigger choice: NOTE CONTINUE normalized 0.5, processor default `TriggerMode::Continue`. Stored v1 trigger=1 migrates to Note Retrigger=2; versioned state is v3.

GUI-only tags: `900 UiZoom`, `901 Clear`, `902 Delete`, `903 Shape`, `904 Snap`, `905 Previous`, `906 Next`, `907 GraphZoom`, `908 PointPosition`, `909 PointPitch`, `910 AddJump`. All have source dispatch in `PitchatorController::valueChanged`. Scale note label updates in `syncScaleNotes`; graph's mouse actions are handled by `PitchCurveView`. Raw existence and dispatch are source-verified; do not confuse with native mouse/automation end-to-end coverage.

| Controls/requirements | Source route | Automated evidence | Release status |
| --- | --- | --- | --- |
| Key, Scale, Scale Lock | 100/101/104; scale note label; pitch engine | Engine, ScaleNoteNames, ParameterFlush, UIContract | Structural PASS; exhaustive native automation NOT VERIFIED |
| Length (1–16 bars) | 102; processor pattern crop; editor synchronization | Engine, Scheduler, ParameterFlush | Structural PASS; editor-closed length automation + project recall NOT VERIFIED |
| Bend Range | 103; scheduler bend14 + GUI graph range | Engine, ParameterFlush | Structural PASS; downstream synth bend-range matching requires host check |
| Start Mode Host/Continue/Retrigger | 105; Scheduler | Scheduler, ParameterFlush including migration | Automated PASS; individual real-host mode comparison NOT VERIFIED |
| Loop, Bypass | 106/107; scheduler and toggle listener | Scheduler, ParameterFlush, UIContract | Automated PASS; DAW automation/reopen each NOT VERIFIED |
| Curve graph add/drag/delete, zero-time jumps | PitchCurveView -> message/CurveMailbox -> processor -> bend output | CurveMouseEvents, Engine, Scheduler, ParameterFlush | Automated PASS; physical all-gestures-at-150% NOT VERIFIED |
| Numeric position, pitch | 908/909 -> PitchPointEdit -> view commit | PitchPointEdit, UIContract | Automated PASS; real DAW focus-loss/keyboard editing both fields NOT VERIFIED |
| Clear/Delete/Curve shape/Snap/Jump | 901–904/910 -> PitchCurveView | UIContract, CurveMouseEvents, UI Layout | Structural PASS; end-to-end per-button host trace NOT VERIFIED |
| Previous/Next/Graph Zoom | 905–907 -> PitchCurveView | UIContract/UI Layout | Structural PASS; physical mouse + boundaries NOT VERIFIED |
| UI zoom 100/150% | 900 -> VST3Editor::setZoomFactor | UI Layout; 125A editor lifecycle | Static PASS; Studio One 150% resize/click test NOT VERIFIED |
| Scale note label | KEY/SCALE -> syncScaleNotes | ScaleNoteNames, UI Layout | Structural PASS; native redraw with automation and zoom NOT VERIFIED |
| Save/load project state | Processor get/setState, Controller setComponentState, CurvePacket v1/v2 | ParameterFlush, CurveMailbox | Automated PASS; user reports Studio One save/reopen PASS |
| Offline/realtime, transport tempo, block-size extremes | process + Scheduler | Existing CTests and 125A tester | Coverage incomplete / NOT VERIFIED as exhaustive host matrix |
| Output MIDI Pitch Bend and event ordering | ProcessData -> bend14 -> MIDI CC pitch events | Scheduler, ParameterFlush, 125A I/O event probe | Automated PASS; audible receiving-synth jump/bend-range matrix NOT VERIFIED |

## Code review findings and gates to close
1. **Documentation mismatch:** `README.md`, `docs/V1_SPEC.md` and `docs/QA_TRACEABILITY.md` contain historical claims that Studio One, state and plugin tester evidence are missing. Replace outdated status lines only once the linked evidence is reconciled; preserve history distinctly from current release state.
2. **Host GUI QA gap:** CI static 100/150% geometry does not prove Studio One's 150% resize/hitboxes, numeric-field focus/commit, Ctrl+click/right-click gesture routing, or each control's automation/reopen. Record physical host tests individually.
3. **MIDI/transport QA gap:** Add explicit evidence for live/offline transport stop/restart, tempo changes, simultaneous voices same MIDI channel, re-centering, discontinuities on note boundaries, and correct downstream synth bend range. Channel-wide (non-MPE) behavior is intentional.
4. **Realtime measurement gap:** `process` uses a per-sample bounded loop and a 16-channel scheduler; source review shows fixed arrays and a non-blocking mailbox snapshot, but no recorded callback mean/p95/p99/max or overrun-count under maximum event/pattern load. Do not claim measured deadline assurance.
5. **Release evidence gap:** current run's prototype artifact has been uploaded, but an independently verified candidate ZIP hash, final contents manifest, actual Gumroad package and Final repository are not yet established for this exact build.

## Audit release gate
No Final repository transfer or release-ready claim until relevant points above are PASS with evidence or explicitly scoped out with a justified non-applicability note. Follow `125A-Engineering/ENGINEERING-GUARDRAILS.md` and `QA/RELEASE-QA.md`. Bundle tests and any corrections; avoid unnecessary CI cycles.

## Audit reinforcement recorded after the successful baseline
- `tests/UILayoutTests.cpp` now checks all view children against their **immediate parent** container bounds rather than only root screen bounds. A static preflight of 39 parsed UIDESC views found **0 nested overflow cases**; CTest execution of the added assertion is still pending.
- `tests/UIContractTests.cpp` now includes GUI action 910 (JUMP TO 0) in the mandatory interactive tag inventory. Source confirms its UIDESC entry and controller handler; automated rerun is pending.
- `README.md`, `docs/V1_SPEC.md` and `docs/QA_TRACEABILITY.md` now prominently identify current QA evidence while retaining earlier development notes as history.
- **No new binary code changed during this audit batch.** A final CI run is required for the updated tests before a release candidate can be marked fully regression-verified.

## Confirmed safety defect identified during audit (fixed in source, CI pending)
- On input-event count exceeding the 2048-event processing buffer, the original fallback called `scheduler_.reset()` without first transmitting a center Pitch Bend to channels already bent in a previous block. A downstream synth could consequently stay detuned.
- Production fix `d402a94`: center existing channel bends at offset 0 before the emergency MIDI passthrough/reset; the source input events remain forwarded.
- Regression test `4b46c6c`: generate a noncenter bend, feed 2049 input MIDI events, check reset event value 8192 and all 2049 forwarded events.
- **Status: corrected, NOT YET CI-VERIFIED.** No release claims until updated test, validator and 125A suite all pass. Further hostile-input and automation edge cases remain part of audit.

## Existing successful artifact metadata (not the corrected build)
- Run 38329739159 prototype artifact ID `11782369102`, GitHub-reported ZIP archive size 1,644,891 bytes, not expired.
- QA diagnostics artifact ID `11782344338`.
- Independent extraction, bundle manifest verification and cryptographic hash for the final **corrected** candidate remain pending and must not reuse Build #23's hash.

## Updated CI verification — 2026-10-13
- Latest Windows QA run: https://github.com/challanger2000/125A-Pitchator/actions/runs/38331057978 — **SUCCESS**.
- CTest **9/9 PASS**, including the new 2049-event overflow/reset regression, expanded immediate-parent GUI clipping check, and mandatory AddJump control tag inventory.
- Steinberg Validator **47/47 PASS**.
- 125A Plugin Tester Final v0.2.9 **PASS**; I/O Event Probe and Editor Lifecycle Probe both report **Result: PASS**.
- Branded VST3 artifact upload succeeded.
- The former source-fix CI-pending blockers are now **verified closed**; any earlier 'CI pending' language in this audit describes the historical pre-run state.
- **Still not a complete FINAL gate**: missing explicit host-level 150% resize/click matrix and comprehensive per-control automation, transport/tempo and downstream synthesizer checks remain separately NOT VERIFIED. No new code changes are warranted without finding a specific defect.

## Studio One host acceptance — user confirmation (2026-10-13)
- User reports the current Pitchator V1.0.0 works correctly in their DAW (Studio One): **user host acceptance PASS**.
- Prior user feedback confirmed MIDI output, GUI interaction and project save/reopen. This latest statement confirms no outstanding observed DAW defect.
- This is a user-reported acceptance result, not an instrumented per-control screenshot/automation test report. Keep the distinction from automated coverage; do not label unobserved detailed test cases independently PASS.
- Windows automated release gates remain PASS in run 38331057978 (9/9 CTests, 47/47 Steinberg validator, 125A Plugin Tester including editor lifecycle and I/O event probe).
- V1.0.0 Final repository source tree was transferred byte-for-byte (30/30 blobs matched) from development commit 6cb852758a215bfa3e67962d5baae6fa3f3674d1. No DSP/code changes from this documentation update.
