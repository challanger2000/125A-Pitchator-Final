# 125A Pitchator V1 — binding product contract

## Current audit status (2026-10-13)

V1 is implemented as a development VST3 prototype and successfully passed
[Windows Actions #38329739159](https://github.com/challanger2000/125A-Pitchator/actions/runs/38329739159)
(9/9 CTests, Steinberg Validator, 125A Plugin Tester). The original scope
and earlier prototype status passages below are retained as historical
requirements, NOT present-day claims. Actual 3-choice Start Mode default
is NOTE CONTINUE; current state schema is v3. The outstanding FINAL
verification gates are tracked in [RELEASE_AUDIT_V1.md](RELEASE_AUDIT_V1.md).

**Project state:** architecture and MIDI pitch-curve core in development. NO release claim.

## Product
Windows x64 Steinberg VST3 MIDI effect, not an instrument and not an audio processor.
Input MIDI notes pass through; output contains standard MIDI channel pitch-bend (14-bit).
No MPE and no independent per-note pitch for simultaneous notes on one channel.
A downstream synth must be configured to match the plugin's bend range.

## Timeline & editor
1–16 bars. V1 musical grid: 4/4, up to 1/32 resolution, project/transport synchronization.
Every point specifies (tick, pitch offset in semitones, interpolation to next point).
Point 0 is mandatory at 0 st; right/left click, drag, delete, exact numeric editing,
scroll, zoom, clear/reset, bypass and 100/150% interface zoom are required GUI tasks.
Transitions Step, Linear, Smooth. User example: 0 → +3 st → +1 st → 0.
Points are editable independently of tempo and sample rate.
The curve may loop, or play once. Modes: HOST SYNC and NOTE TRIGGER.
In NOTE TRIGGER, an incoming Note-On starts at phase 0; actual sample offsets matter.
The source MIDI note is never rewritten to implement bending.

## Tonal semantics
SEMITONE mode: offsets are literal, e.g. +3 then -2 RELATIVE to current
destination gives +1 relative to the originally played note.
SCALE LOCK: target pitches snap to notes belonging to KEY/SCALE, with deterministic
tie-breaking in travel direction. 0 st stays 0, even when a user's played
source note is outside the configured key. A continuous glide may pass through
other pitches; SCALE LOCK constrains the destinations only.
KEY: 12 pitch classes. Scales: chromatic, major, natural/harmonic minor,
major/minor pentatonic, dorian, phrygian, lydian, mixolydian, locrian.
BEND RANGE must match the receiving synth and the 14-bit signed/asymmetric
mapping must preserve center 8192 and endpoints 0 / 16383.

## Host and lifecycle
Forward original NoteOn/NoteOff, velocity, channel, note ID and sample offset.
Honor host transport PPQ and stop/restart, same-block automation, offline rendering,
project recall, bypass and panic/reset events.
Do not fabricate an output MIDI pitch bend when processor is bypassed.
Always return bend to center when stopping, bypassing, clearing and ending
NOTE TRIGGER, with defined overlap/channel behavior.
Generate bounded events, realtime-safe; no heap/locks/files/formatting in callback.
Tests must check actual output events, not just curve evaluation.

## Version and engineering rules
Use latest verified stable SDK 3.8.1 tag v3.8.1_build_84 (2026-08-11),
C++17, official Steinberg VST3 interfaces and VSTGUI.
Product static design + approved 125A-Branding master before custom VSTGUI code.
Do not develop on main. Bundle commits/CI. No false FULL AUDIT claims.
Final release: Steinberg Validator, 125A Tester, editor lifecycle, exact event
routing, Studio One host playback and complete UI/parameter/end-to-end audit.

## Historical scope statement
The first commit did not contain the VST3 wrapper, GUI or state serialization.
These components have since been implemented and Windows-compiled in the
development branch. The hostless event-list tests and validator are green,
but practical Studio One/GUI lifecycle/125A Tester evidence is still missing.

## Numeric editing contract (V1 extension)
Use BAR.BEAT.TICK (e.g. 2.3.120; ticks 0–959 inside each beat);
960 PPQ/quarter, 3840 PPQ/bar. User-facing values start at bar 1,
beat 1, tick 000. The curve stores exact ticks and the mouse Snap
supports 1/32 notes (120 ticks), without rounding typed positions.
Type pitch as signed semitones with decimal dot or German decimal
comma; all edits reject invalid/out-of-range values, cannot move
the immutable point 0 or silently replace a point at an occupied time.

## V1 trigger extension: NOTE CONTINUE (2026-10-13)

Start Mode now exposes HOST SYNC / NOTE RETRIGGER / NOTE CONTINUE.
NOTE RETRIGGER always restarts at Bar 1 on each NoteOn. NOTE CONTINUE starts
on the first NoteOn and persists its per-channel phrase clock across NoteOffs
and short rests: a new note at Bar 2 plays Bar 2 of the *same* curve. After
one full LENGTH has elapsed, the next NoteOn starts a new phrase at Bar 1.
Loop ON repeats the curve while notes sustain; Loop OFF ends at Length and
centers the bend. Pitch Bend remains MIDI-channel-wide (no MPE). The previous
saved-state trigger values 0/1 must remain HOST SYNC/NOTE RETRIGGER; new
states use version 2 and allow value 2 (NOTE CONTINUE). The VST3 parameter
ID 105 is preserved; its new 3-choice normalization is 0 / 0.5 / 1.

### Compatibility refinement

To retain **old host automation** on ParamID 105, normalized 1.0 must
continue to mean NOTE RETRIGGER. Therefore the actual V2 menu ordering is
HOST SYNC (0), NOTE CONTINUE (0.5), NOTE RETRIGGER (1.0). The old raw
state-v1 trigger 1 migrates to internal state-v2 trigger 2 on read, without
changing its musical effect. This prevents already saved MIDI NOTE
host-automation lanes from silently turning into NOTE CONTINUE.

## Dual knots at identical timestamp (2026-10-13)

A time position may contain one point or an ordered pair: **arrival**
(end of previous ramp) and **departure** (beginning of next ramp).
Same-tick pairs generate a real instantaneous MIDI pitch-bend jump;
they are *not* resampled at neighboring ticks. Examples: BAR 1 rising
0→+6 while BAR 2 starts at exactly 0, or saw/zigzag shapes with sudden
changes at arbitrary beats. No triple knots allowed; start tick 0 stays
neutral. Each knot retains its own Curve type and individually selectable
pitch, position and deletion. Shift+LeftClick on an existing X creates
another knot at the click Y. CTRL+LeftClick deletes without relying on
Studio One to forward right-click; Delete Point remains available. A new
JUMP TO 0 button duplicates a selected time knot with 0 semitones and
selects the outgoing knot. Native right-click deletion is retained.

State evolves from version 2 to **3**, retains version 1/2 compatibility.
GUI binary CurvePacket magic evolves PCV1 → PCV2, while legacy PCV1
snapshots remain readable. Real-time mailbox retains knot ordering and
no allocations/locks occur in audio process. Scheduler calculates next
jump sample and emits discontinuities sample-accurately (rather than
moving it to the 64-sample periodic tick). All tests must pass before
Studio One host approval.
