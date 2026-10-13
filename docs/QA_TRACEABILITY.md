# 125A Pitchator V1 — QA control & requirement traceability

## Current release-audit overlay (2026-10-13)

The table and early-prototype notes immediately below are **historical** and
must not be read as current implementation status. The up-to-date control
inventory, source routes and per-gate PASS / NOT VERIFIED assessment is in
[RELEASE_AUDIT_V1.md](RELEASE_AUDIT_V1.md). Latest binary evidence:
[Windows Actions #38329739159](https://github.com/challanger2000/125A-Pitchator/actions/runs/38329739159),
9/9 CTest, Steinberg Validator and complete 125A Plugin Tester PASS. The
user reports Studio One MIDI routing and project save/reopen PASS.
Independent real-host per-control traceability remains open before FINAL.

Legend: **CORE TEST PASS** = locally defined deterministic assertions only;
**NOT VERIFIED** = no valid end-to-end evidence yet. Never equate core tests
to VST3 validator, 125A tester or Studio One acceptance.

| Requirement / GUI control | Processor / action | MIDI or state consequence | Evidence/status |
| --- | --- | --- | --- |
| Curve point insert / drag / delete | Pattern::upsert/erase | Target pitch recalculation | Core test defined; host NOT VERIFIED |
| Clear Pattern | Pattern::clear | Neutral 0st curve + MIDI reset TBD | Core test defined; host NOT VERIFIED |
| Taktzahl 1–16 | Pattern::setBars | Duration in MIDI PPQ ticks | Core test defined; host NOT VERIFIED |
| Key / Scale | scale masks + snapTarget | Scale-correct destination | Core test defined; host NOT VERIFIED |
| Semitone / Scale Lock | value() | Literal / snapped destination | Core test defined; host NOT VERIFIED |
| Step / Linear / Smooth | interpolate() | Slope and step of bend | Core test defined; host NOT VERIFIED |
| Bend Range | bend14() | MIDI 14-bit center and endpoints | Core test defined; host NOT VERIFIED |
| Grid / Snap / Scroll / Zoom | GUI TBD | Editor pitch-point time/position | NOT VERIFIED |
| HOST SYNC | hostTick() | PPQ-to-tick phase | Core test defined; host NOT VERIFIED |
| NOTE TRIGGER | event scheduler TBD | Sample-accurate restart | NOT VERIFIED |
| Loop / Once | event scheduler TBD | Bounded 1–16-bar playback | NOT VERIFIED |
| MIDI pass-through | VST3 processor TBD | Identical note on/off/order | NOT VERIFIED |
| Standard MIDI Pitch Bend output | VST3 processor TBD | LegacyMIDICCOutEvent 14-bit | NOT VERIFIED |
| Bypass / Panic / Stop | processor TBD | Center pitch + cleanup | NOT VERIFIED |
| GUI 100/150% & mouse hitboxes | VSTGUI TBD | Editor and interaction | NOT VERIFIED |
| Save/Load / Automation / Reopen | VST3 state TBD | Identical restored curve | NOT VERIFIED |
| Validator / Plugin Tester | Release QA | Host compliance | NOT VERIFIED |
| Studio One note, routing, bend | DAW interaction | Musical acceptance | NOT VERIFIED |

All GUI controls require a complete UI tag -> input -> controller -> parameter /
command -> processor -> measured MIDI -> visual feedback -> automation & recall
chain before V1 can be called finished.

## Native prototype after first VST3 adapter batch

The following routes are implemented in C++ but not host-verified:
- VST3 NoteOn/NoteOff forwarding and pitch bend output (VST3 build NOT VERIFIED).
- HOST tempo/PPQ synchronization and NOTE restart (scheduler unit tests defined).
- 14-bit LSB/MSB mapping, bypass centering, note-off centering.
- Processor component state read/write and Controller setComponentState display.
- No GUI control inventory yet because no interactive editor is implemented.
- Lifecycle, host automation, event emission, editor and Studio One tests OPEN.


## VSTGUI prototype binding inventory — STATUS: NOT VERIFIED IN HOST
| Visible control | Tag | Controller / processor route | Evidence required |
| --- | --- | --- | --- |
| Key | 100 | Host parameter -> Scale Lock | DAW click, automation, MIDI target |
| Scale | 101 | Host parameter -> Scale Lock | Mode selection and note target |
| Length | 102 | Host parameter -> cropped timeline | 1/2/16 bars, event clock |
| Bend Range | 103 | Host parameter -> 14-bit encoder | Actual MIDI LSB/MSB |
| Scale Lock | 104 | Host parameter -> snap | Toggle, target pitch |
| Trigger | 105 | Host parameter -> host/note clock | sample-offset NoteOn |
| Loop | 106 | Host parameter -> one-shot/loop | MIDI bend and end reset |
| Bypass | 107 | Host parameter -> 8192 center | On/off, all channels |
| GUI Zoom | 900 | VST3Editor::setZoomFactor | Real editor attach/resize at 100/150 |
| Clear Pattern | 901 | CurveView::clearCurve -> message -> mailbox | Clear while stopped, save/reopen |
| Delete Point | 902 | CurveView::eraseSelected -> message | Last/first point & output |
| Curve Shape | 903 | CurveView::setCurve -> message | Step/Linear/Smooth output |
| Snap | 904 | CurveView::setSnap | GUI gesture, snapped tick |
| Previous/Next | 905/906 | CurveView::scrollBars | 16-bar view and click alignment |
| Graph Zoom | 907 | CurveView::cycleVisibleBars | Accurate viewport geometry |
| Curve surface | curve | mouse events -> CurvePacket -> mailbox | add/move/delete, MIDI, state |

Controller / mailbox / processor C++ routes exist in the development source.
The static UI contract test checks for matched tags and code paths only.
**No UI click is considered PASS until actual host events and MIDI effects
are observed.** The current prototype remains NOT RELEASE READY.

## CI Evidence — 2026-10-12

Build [#5](https://github.com/challanger2000/125A-Pitchator/actions/runs/38253823977)
for commit `6a39f02da974b96659d00ab320fd3331d35d4a3f`:
- MSVC Windows x64 Pitchator VST3: **BUILD PASS**
- `PitchEngineTests`, `PitchSchedulerTests`, `CurveMailboxTests`,
  `PitchatorUIContractTests`, `PitchatorParameterFlushTests`: **5/5 PASS**
- Steinberg VST3 Validator: **47/47 PASS** on the built Windows bundle
- Prototype artifact: **Pitchator-Windows-VST3-Prototype**, artifact ID
  `11749541953`, archived by GitHub Actions
- BYPASS persistence defect: FIXED; zero-buffer input parameter queues
  consumed before the no-outputEvents return. Direct regression reproduces
  validator preset save, bypass reset and controller restore.

**NOT VERIFIED and still blocking FINAL:** real Studio One MIDI routing and
actual pitch behavior on a receiving VSTi; 125A Tester; functional GUI
click/drag/zoom/reopen; every-control state/automation traceability;
sample-accurate bend curves through transport jumps, host seeking and offline
rendering; mouse hitboxes at 100/150%; packaging and customer documentation.
Static UI test is not a substitute for a real host click test.

## Numerical curve-editor milestone — source integrated, host verification open

- Exact selected-point POSITION field: BAR.BEAT.TICK (960 PPQ),
  1-based bars/beats, tick 0–959, across 1–16 bars.
- Selected-point PITCH field: decimal semitones ±48 with German comma/dot.
- Mouse Snap now aligns to 1/32 (120 ticks); no-snap freehand positions
  retain full PPQ tick precision.
- Invalid numbers and duplicate point positions are rejected without
  replacing or deleting existing points; point 0 stays fixed at (0, 0 st).
- Selection refreshes both fields after mouse add/drag/delete, Clear,
  project restore and Length change. Curve display follows chosen bend range.
- New deterministic `PitchPointEditTests`; GUI routing also checked
  in `PitchatorUIContractTests`. These do **not** prove native GUI click
  behavior or Studio One MIDI routing.

**No FINAL designation until real-editor lifecycle, Studio One and
125A Tester results are available.**

## Native event-route integration test — added after build #6

`PitchatorParameterFlushTests` now includes a hostless VST3 event-list
probe using the real SDK `HostMessage`, `EventList`, `ProcessData`
and `ProcessContext` classes. It sends a full curve packet through
`IConnectionPoint::notify`, rejects a malformed packet, and asserts
that a MIDI note on channel 3 with note ID 42 is forwarded unchanged
while a -3-semitone 14-bit channel pitch bend arrives **before** the
NoteOn and pitch returns to center **after** NoteOff at the same
sample position. Also checks serialized/reopened processor state.

This establishes SDK object-level event evidence, but not actual Studio One
routing or physical GUI click/drag behavior. Status remains pending
successful Windows CI for this test.

## Windows QA — Build #7 (2026-10-12)

Workflow: https://github.com/challanger2000/125A-Pitchator/actions/runs/38256289397
Commit: `ec927c590771ce29355e204fe4b80af39f04e2d2`.
- Windows x64 / Steinberg SDK 3.8.1: **BUILD PASS**.
- **6/6 CTest PASS**, including exact numeric entry and
  `PitchatorParameterFlushTests` with real SDK MIDI `EventList`,
  `HostMessage`, `ProcessData` and state restoration.
- **47/47 Steinberg Validator PASS** (six tests do not replace the 47 validator gates).
- GitHub Actions prototype artifact `11750955918` generated from the
  validated commit.
- On the SDK input/output event-list test, a GUI-equivalent -3-st curve
  message was accepted, malformed packets rejected, channel 3 pitch bend
  was delivered before original NoteOn (ID 42), followed by original
  NoteOff and center bend at offset 128. Processor state reopened.
- **NOT YET VERIFIED:** Studio One MIDI routing/receiving synthesizer,
  GUI mouse hitboxes/resize 100%/150%, repeated editor lifecycle,
  125A Tester and full practical release audit. Do not mark FINAL.

## Full automated 125A release QA — Build #8, 2026-10-12

**Run:** https://github.com/challanger2000/125A-Pitchator/actions/runs/38257844184

**Tested commit:** `e15f2554ff4171d1c21e4dce7725d118129d72a8`

The validated Windows x64 VST3 binary from this one run passed:
- **6/6 CTest PASS**.
- **47/47 Steinberg VST3 Validator PASS**.
- **125A Plugin Tester v0.2.9: 28 PASS / 0 WARNING / 0 FAIL**.
- Native editor lifecycle: **5 full create / attach / runtime / detach /
  destroy cycles PASS**, according to the 125A Editor Lifecycle Probe.
- 125A isolated event & I/O probe: **PASS, FirstFailure: None**.
- Tester also exercised process mode, state recall, parameter automation,
  processing format, bypass and other built-in release QA gates.
- Exact validated Windows VST3 ZIP available as Actions artifact:
  `Pitchator-Windows-VST3-Prototype`, artifact ID `11751885229`.
- Exact Tester report package:
  `Pitchator-125A-Release-QA-Diagnostics`, artifact ID `11750689704`.
- All QA reports archived; none of the automated test results were inferred
  solely from compiling or static source searches.

**Remaining NON-AUTOMATED gates:** Studio One end-to-end MIDI routing to a
real receiving VST instrument; real-world audible pitch behavior with
matching synth bend range; GUI control click/drag feedback, 100/150% visual
hitboxes and zoom in the DAW, DAW automation/transport and reopen/restore;
user's musical approval. The native editor lifecycle probe is real runtime
evidence, but it is not a Studio One click/interaction test.

**Release status: ENGINEERING TEST CANDIDATE — not Gumroad FINAL.**

## Studio One report — VST3 factory category fix, 2026-10-12

User observed VST3 shown under **FX**, not **Instruments**.
Code review confirmed `DEF_CLASS2` used `"Fx|Tools"` although the
125A Midiator registers as `Vst::PlugType::kInstrumentSynth`.
The Pitchator's class metadata has been changed to match the working
125A MIDI instrument registration (`Vst::kDistributable` and
`Vst::PlugType::kInstrumentSynth`), without changing DSP/MIDI
algorithms, event buses, controller class ID or state format.

The static UI/metadata contract now rejects regression to FX category.
**Status pending Windows build/Validator/125A Tester**; host classification
must be independently verified by a fresh Studio One plugin scan.
Do not mark Studio One Instrument route PASS from metadata alone.

The same next build also verifies the corrected artifact ZIP structure:
`Pitchator.vst3/Contents/...`.

## Confirmed Instrument + packaging fix — Windows Build #9

- Tested commit: `012c16fedc2a88e689f3718ada5ef63c5df6a9cf`
- GitHub Actions run: https://github.com/challanger2000/125A-Pitchator/actions/runs/38293793022
- **6/6 CTest PASS; 47/47 Steinberg Validator PASS; 125A Tester 28 PASS /
  0 WARNING / 0 FAIL** (including 5 native editor lifecycle cycles and
  I/O event probe).
- Official Windows VST3 artifact ID `11767334386`.
- Verified the **downloaded ZIP bytes**, not just uploader success:
  `Pitchator.vst3/Contents/Resources/moduleinfo.json` present;
  `Pitchator.vst3/Contents/x86_64-win/Pitchator.vst3` present;
  ZIP CRC validation passed.
- `moduleinfo.json` generated by the VST3 factory reports
  `"Sub Categories": ["Instrument", "Synth"]` and **Class Flags 1**;
  the previous `Fx/Tools` classification is absent.
- Remaining Studio One category scan and MIDI routing checks are
  human-host acceptance gates; **not** asserted from moduleinfo alone.
- Workflow back to **manual-only**, no Action on documentation update.

## Studio One GUI bug — user report 2026-10-12

The user could not draw/edit curves. Code traced to double-transforming
the mouse position in `PitchCurveView::pointerX/Y`: the VSTGUI event is
parent-local already, but code called `frameToLocal()` and added
`getViewSize().left/top` again. In Studio One the resulting Y coordinate
lies below the graph's actual hit region. MouseDown was ignored.
Fix: use VSTGUI parent-local mouse positions unchanged, matching
`getViewSize()` and drawing coordinates. Add native VSTGUI mouse-event
regression that asserts click insert, drag, select, right-click delete,
origin immutability and out-of-bounds rejection at real graph bounds.
After build, real Studio One physical GUI test remains NOT VERIFIED.

## Complete first-pass GUI-control audit after user feedback (2026-10-12)

- USER-REPORTED FAIL: Scale Lock, Loop, Bypass do not operate in Studio One.
  Source previously relied on implicit VST3Editor parameter listeners; these three
  CTextButtons are now explicitly bound to controller gesture begin/perform/end.
  ON and OFF each send a normalized parameter edit, with UI state synchronized
  after host automation and project restore.
- USER-REPORTED FAIL: Pitch Curve labels disappear when clicking controls.
  The custom graph now calls `CDrawContext::setFont(kNormalFontSmall)` for every
  repaint; title/background and text contrast are also checked. This is a
  suspected redraw issue and requires real Studio One confirmation.
- All CTextButtons now have dark text on light default idle gradients and
  light text on highlighted gradients, instead of unreadable light-on-light.
- Requirement scope: Key/Scale/Start Mode native VST3 parameters; Length/Range
  slider+text native automation; three dedicated toggles; zoom, clear,
  delete, curve shape, snap, previous/next bar, graph zoom, numeric position,
  numeric pitch, graph left/add/drag/right/delete, 100%/150% zoom.
- HOST interaction NOT VERIFIED until user confirms physical click, displayed
  state, MIDI consequence, 100/150% zoom and save/reopen.
- Build #10 failed the new mouse-event test (uninitialized standalone VSTGUI
  platform, 6/7 tests passed). Test harness now initializes and exits VSTGUI
  with the SDK platform lifecycle; do not mark PASS until next CI proves it.

## Native CTextButton regression test (added after Build #10)

Extended `PitchatorParameterFlushTests` executes real VSTGUI MouseDown/Up
on each of Scale Lock, Loop and Bypass, verifies the controller receives
click ON and click OFF, and verifies direct setParamNormalized restoration
is reflected in the button. Also tests that the original BYPASS state from
the validator is not broken. First run pending.

## Build #11 — actual source-level control traceability (2026-10-12)

Workflow: https://github.com/challanger2000/125A-Pitchator/actions/runs/38296952444
Tested commit: `a2230df10aa7fd35636d26d7fbb9590cdf34f84b`.
**7/7 CTest PASS, 47/47 Validator PASS, 125A Tester 28 PASS/0 WARN/0 FAIL.**
Windows x64 VST3 artifact ID `11768729087`; QA report artifact ID
`11768289214`. Native Editor Lifecycle: 5/5 cycles PASS. Full artifact ZIP
was independently downloaded and CRC checked, and contains
`Pitchator.vst3/Contents/x86_64-win/Pitchator.vst3` plus resources.

| Visible UI group | Control-path verification | Host status |
| --- | --- | --- |
| Key, Scale, Start Mode | Native VST3Editor parameter tags 100,101,105; scale/trigger processor and MIDI scheduler unit regression | Physical Studio One click NOT VERIFIED |
| Length/Bend Range sliders and text | Native VST3Editor tags 102/103; controller setParamNormalized -> processor changes -> saved state | Host movement NOT VERIFIED |
| Scale Lock | Real CTextButton MouseDown/Up -> on/off controller listener -> host begin/perform/end -> param 104 -> processor ScaleLock; automation/reload updates title and value | Native gesture PASS; Studio One NOT VERIFIED |
| Loop | Real CTextButton MouseDown/Up -> on/off controller listener -> host begin/perform/end -> param 106 -> processor scheduler Loop; automation/reload updates title and value | Native gesture PASS; Studio One NOT VERIFIED |
| Bypass | Real CTextButton MouseDown/Up -> on/off controller listener -> host begin/perform/end -> param 107 -> processor bypass/center + state recall | Native gesture PASS; Studio One NOT VERIFIED |
| UI Zoom | Native kick MouseDown/Up -> controller tag 900 -> 100/150 factor + title | Native gesture PASS; Studio One scaling NOT VERIFIED |
| Clear / Delete | Native kick MouseDown/Up -> tags 901/902 -> Pattern::clear/erase -> publish to processor | Native gesture PASS; Studio One NOT VERIFIED |
| Curve Shape / Snap | Native kick MouseDown/Up -> tags 903/904 -> Step/Linear/Smooth, 1/32 snap | Native gesture PASS; Studio One NOT VERIFIED |
| Prev/Next Bar, Graph Zoom | Native kick MouseDown/Up -> tags 905/906/907 -> scroll bars/visible range | Native gesture PASS; Studio One NOT VERIFIED |
| Position, Pitch | Native numeric controls tags 908/909, validation/edit selected node/reject collision | Controller-route PASS; actual keyboard typing NOT VERIFIED |
| Graph pointer | Real VSTGUI event simulations: select/add/drag/right-click delete; offset conversion corrected, 0-point immutable | Native gesture PASS; physical Studio One NOT VERIFIED |
| Graph text redraw | Explicit font state before drawing every frame and corrected idle/active button text colors | Visual Studio One redraw NOT VERIFIED |

The above **does not** claim complete physical DAW interaction or GUI final signoff.

## Static UI layout correction (2026-10-12; build pending)

Rebuilt 1200x620 GUI grid from user's Studio One screenshot:
- Header logo official master at (30,12), 148x74; main heading and
  subtitle share center x=600; zoom at (1055,30), size 110x34.
- Top settings panel x=24,y=96,w=1152,h=112. Eight groups:
  KEY x16/w80; SCALE x114/w170; LENGTH x304/w152; RANGE x476/w152;
  START x647/w158; LOCK x817/w105; LOOP x933/w90;
  BYPASS x1033/w105. Caption y24/19; option/input/button y52/32;
  sliders y60/16. No clickable overlap; minimum inter-group gap 10px.
- Removed the default black opaque label rectangles; all labels are
  transparent, using centered Caption font for consistent hierarchy.
- Graph and pitch inspector retain 1140x267 chart area, y248; input
  controls at y520. Footer commands now share y556/h32 with 10px gaps
  and intentional left/right clusters.
- Logo master exactly matches current 125A-Branding SVG; an exact
  2811-command vector renderer uses even-odd fills, no font redraw.
- Unselected/selected node fills now use blue variations, graph line
  retains turquoise. MIDI engine and state remain unchanged.
- Added fail-fast 8th CTest to validate text-width headroom, collisions,
  baselines, correct logo and 100/150% extents. **This is NOT a Studio
  One physical GUI PASS**; native code build and user inspection pending.

## Build #13: confirmed layout engineering QA, 2026-10-12

- Full workflow: https://github.com/challanger2000/125A-Pitchator/actions/runs/38310480336
- Tested HEAD: `bca28c71187253c6c78197af532bc844a8364398`
- **8/8 CTest PASS**, including new `PitchatorUILayoutTests` for
  geometry, text headroom, logo master derivation and 100/150% bounds.
- **47/47 Steinberg VST3 Validator PASS**.
- **125A Plugin Tester v0.2.9: 28 PASS, 0 WARNING, 0 FAIL**, including
  five editor lifecycle cycles and native MIDI-I/O probe.
- VST3 prototype artifact ID `11774014991`, diagnostics ID
  `11775265058`.
- ZIP independently downloaded, root is exactly `Pitchator.vst3`,
  `Contents/x86_64-win/Pitchator.vst3` present and CRC OK.
- The prior Build #12 failure was missing `BrandLogoView.cpp` from the
  plugin and test-harness CMake source lists (LNK2001). Both targets now
  include the renderer. This is a build-configuration mistake, **not**
  evidence that Studio One rendering is already acceptable.
- **Additional pre-FINAL packaging check:** the SDK-generated
  `moduleinfo.json` contains trailing commas, which strict JSON parsers
  reject. The VST3 validator accepted this bundle, but strict machine-readable
  metadata should be repaired and revalidated before final distribution.
- **Still NOT VERIFIED:** actual Studio One alignment, logo appearance,
  100/150% zoom, physical mouse hitboxes and musical MIDI playback in host.

Automatic CI trigger restored to **workflow_dispatch only**.

## SDK package naming + Note-On retrigger semantics (2026-10-13)

- Studio One user confirms mismatch: VST3 bundle was
  `Pitchator.vst3`, desired `125A Pitchator.vst3`.
- Updated SDK `smtg_add_vst3plugin(Pitchator PACKAGE_NAME "125A Pitchator"
  SOURCES_LIST ...)` and asserted built target's
  `SMTG_PLUGIN_PACKAGE_NAME == "125A Pitchator.vst3"`.
- Updated validator, QA, staging and archive paths to require the real
  SDK output, with a guard against the old `Pitchator.vst3` directory.
- Host factory Class ID and plugin display name remain unchanged to avoid
  unnecessary existing-project compatibility changes.
- Source confirms NOTE Trigger reanchors on **every NoteOn** on that MIDI
  channel, including overlapping notes. HOST SYNC follows host PPQ and
  starts each new note at the current curve position; this behavior was
  in the binding V1 spec and has not been altered.
- Build/QA on renamed package **PENDING**.

## User reports 2026-10-13 — NOTE CONTINUE / right click / package name

- Third Start Mode `NOTE CONTINUE` added after user approved persistent
  phrase clocks across NoteOn/NoteOff transitions; restart only on next
  note after 1–16 bars elapsed. Distinct from NOTE RETRIGGER and HOST SYNC.
- State version 2 writes trigger ID 2; version-1 project states remain
  compatible and preserve the old 0/1 semantics. Parameter ID 105 unchanged.
- Node right-click hit radius increased from sqrt(130) ≈11.4px to 18px;
  regression now right-clicks a point from outside its drawn 6px radius.
- Earlier native SDK `PACKAGE_NAME \"125A Pitchator\"` caused Windows
  SDK resource copy failure at build #14: quoted name broken during
  copy_if_different. Use standard internal `Pitchator.vst3` at build time,
  then create the complete branded directory AND x64 module names in a
  staging step **before** both validators and packaging. Do not claim PASS
  for these changes until the staged VST3 passes all CI/125A checks.

## Build #15 regression findings (2026-10-12)

Build #15 compiled cleanly, but only 6/8 CTest PASS. Failures were
(1) speculative right-button release-only deletion, which VSTGUI does not
advertise as a reliable gesture, and (2) the standalone migration test
left BYPASS off for the following existing ON-state test. Resolution:
remove speculative right-release deletion and retain native right-button
**down** handling with 18px hitbox, while explicitly restoring the initial
BYPASS snapshot before old ON/OFF control checks. Native tests will be
repeated; do not mark PASS from the failed run.

## Legacy host automation compatibility refinement

The new three-choice trigger list must **not** reinterpret old normalized
1.0 NOTE RETRIGGER automation as CONTINUE. The final three values are
HOST SYNC 0, NOTE CONTINUE 0.5, NOTE RETRIGGER 1.0. V1 serialized trigger
1 is migrated at read time to V2 enum 2. V2 written state values 0/1/2
retain their new meanings. Unit and VST3 controller migration tests assert
both the legacy 1.0 and new Continue 0.5 semantics. Await new CI confirmation.

## Final automated evidence for NOTE CONTINUE and branding — Build #17

Run: https://github.com/challanger2000/125A-Pitchator/actions/runs/38317531984
Tested commit `f00d25dc06abeb245232e71c358d8e39301795e0`.

- Windows x64 SDK 3.8.1: **BUILD PASS**.
- **8/8 CTest PASS**, including MIDI scheduler two one-bar notes, phrase
  restarts, legacy V1 state and normalized automation compatibility,
  direct GUI mouse/button interaction and layout geometry.
- **47/47 Steinberg Validator PASS** on the staged branded VST3.
- **125A Plugin Tester: 28 PASS / 0 WARNING / 0 FAIL** on the same bundle,
  including native editor lifecycle and MIDI event probe.
- Downloaded artifact ID `11777897198`, 1,638,932 bytes; ZIP CRC PASS.
  All six archive entries start with `125A Pitchator.vst3/` and the actual
  binary is `Contents/x86_64-win/125A Pitchator.vst3`. No unbranded
  `Contents/x86_64-win/Pitchator.vst3` is present.
- User-reported right-click deletion: 18px native pointer-down target
  and preservation of neighboring nodes regression PASS, but right-click
  propagation in Studio One itself **UNVERIFIED**.
- New triggering mode NOTE CONTINUE: cross-NoteOff phrase clock
  and restart after LENGTH test PASS. DAW receiving-synth bend range and
  musical integration **UNVERIFIED**.
- Manual-only GitHub Actions restored after all checks. No new CI run.

**Gate status:** native and validator QA PASS; physical Studio One host
interaction and user acceptance NOT VERIFIED; not Gumroad FINAL.

## Scale note reference display (2026-10-13; QA PENDING)

New informational `NOTES:` line inside unused bottom 20px of the settings panel.
KEY and SCALE changes recompute the international spelling from the **same
isInScale() mask as the DSP**. C Natural Minor shows `C D E♭ F G A♭ B♭`;
C Harmonic Minor uses B natural, and C Major uses E natural. `SCALE LOCK`
does not control this theoretical reference row; it only affects output
pitch quantization. No new VST parameter, state version or DSP modification.
The line refreshes on controller setParamNormalized, editor creation and
state restore; cleared on editor close. New `ScaleNoteNamesTests` covers
all 132 key/mode combinations and compares pitch classes to `isInScale`.
Native controller integration tests cover key/scale edits and recall. New
line's geometry is checked at 100/150% by UI Layout Tests. Real Studio One
visualization and refresh **NOT VERIFIED** until physical DAW test.

## CI Build #18 compiler correction (2026-10-12)

Build #18 compiled the main Pitchator VST3 but C++ integration probe failed
because the new independent ScaleNoteNames header was referenced without
being included in `tests/ParameterFlushTests.cpp` (MSVC C2039/C3861). The
missing test-only include is fixed; no engine, GUI or ABI behavior changed.
The next Windows run must pass all tests before any artifact is offered.

## Build #19: scale note reference full automated QA (2026-10-12)

- Run: https://github.com/challanger2000/125A-Pitchator/actions/runs/38320920647
- QA-tested commit: `0f2077699e2226782831226725a711a77d58f102`
- Windows VST3 compiled. **9/9 CTest PASS**, including the 132-key/scale
  spelling matrix, VST3 controller KEY/SCALE change and project recall
  regression, geometry and editor/pointer regressions.
- **47/47 Steinberg Validator PASS**.
- **28 PASS, 0 WARNING, 0 FAIL** from 125A Plugin Tester (including
  Editor Lifecycle and MIDI I/O Event probe).
- Artifact `11778533587`: `125A Pitchator.vst3/Contents/x86_64-win/125A Pitchator.vst3`;
  ZIP CRC integrity PASS, 6 entries, correct resource paths.
- Previous failed Build #18 was caused by an integration-test-only omitted
  header include (C2039/C3861). No MIDI or controller behavior changed for
  the fix. Build #19 is the verified tested version.
- KEY/SCALE notes are **informational regardless of SCALE LOCK**; enabling
  the lock still controls the existing MIDI pitch-bend target quantization.
- Actions restored to **manual only**; no additional billable CI build.
- **Studio One human GUI QA OPEN:** verify new note row's legibility at
  100/150%, changing KEY/SCALE live and project save/reopen. Not FINAL.

## Dual point/JUMP engineering coverage (2026-10-13; QA PENDING)

- Explicit ordered two-knot discontinuity at same tick, no epsilon hacks.
- New CurvePacket magic PCV2 with PCV1 backward compatibility.
- VST3 state version 3; reads historical states 1/2. Old parameter IDs,
  controller class IDs and automation normalizations unchanged.
- New GUI JUMP TO 0 action (tag 910), Shift+Left to form pair and
  CTRL+Left to delete when DAW intercepts right-click. Two dots render
  exactly aligned in X, with vertical link. Independent selection/delete.
- Scheduler anticipates and emits discontinuous bends on their exact
  first applicable sample; normal smooth curves retain old rendering cadence.
- Regression tests cover arbitrary zigzag timings, V1/PCV1 migration,
  corrupt packets/triples, mailbox snapshots, selected-knot editing,
  host-style click simulation, UI layout and project restore.
- Studio One real host interaction & audibility **NOT VERIFIED**.

## Build #21 tests (2026-10-12)

The real Windows VST3 and new engine compiled successfully, 7/9 CTest
passed. PitchEngineTests/PitchSchedulerTests/CurveMouseEventsTests,
UILayout and scale reference were PASS. Two test-harness corrections:
(1) PCV2 intentionally accepts two same-tick knots, so the historical
CurveMailboxTests rejection assertion was changed to test PCV1 rejection
and PCV2 ordered-pair acceptance; (2) ParameterFlushTests instantiated
multiple very large VST3 Processor objects as local variables, including
two new state-restore regression probes, exhausting the default Windows
EXE stack. All Processor test fixtures now live on the heap through RAII
std::unique_ptr, without changing production allocation or real-time code.
Full suite, Steinberg and 125A QA remain PENDING after this correction.

## Release-level native QA evidence — dual-point jump Build #23 (2026-10-12)

Actions: https://github.com/challanger2000/125A-Pitchator/actions/runs/38323293624
QA-tested commit `c1bbd27ea352c5af18b4fd4a3d6835578c41806f`.

- **9/9 CTest PASS**: engine duplicate ticks/zigzag at arbitrary beat,
  first sample of MIDI discontinuity, PCV1→PCV2 validation and realtime
  mailbox, two individual GUI dots/Shift+Left/Ctrl+Left/right-delete,
  project state V1/V2/V3 restore, note names, layouts and action buttons.
- **47/47 Steinberg Validator PASS** on the fully staged VST3 package.
- **125A Plugin Tester Final v0.2.9: 28 PASS / 0 WARNING / 0 FAIL**.
- Five editor lifecycle attach/detach cycles, I/O MIDI event probe PASS.
- `Pitchator-Windows-VST3-Prototype` artifact ID `11780045698`.
  Downloaded and independently checked ZIP: 1,644,952 bytes, six files,
  no CRC errors. Every path starts `125A Pitchator.vst3/`. Main DLL
  `Contents/x86_64-win/125A Pitchator.vst3` present, old binary name absent.
  SHA-256 `fad47a917b60b3e5bf45b7d1a2107e01fd86390359d96146fa387d74af516c77`.
- Automatic Actions push trigger disabled again. **No further build needed
  for this documentation-only handoff.**
- **Not yet verified:** physical Studio One mouse gestures, the two dots'
  visual clarity at 100/150% scale, correct audible instant Pitch Bend
  through the downstream synth and full save/reopen host automation behavior.
  These remain user host acceptance gates before FINAL.
