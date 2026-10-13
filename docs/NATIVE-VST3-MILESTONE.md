# Pitchator first native VST3 milestone (prototype, not a release)

This branch implements the VST3 processor/controller shell, event input/output,
native 14-bit MIDI Pitch Bend, fixed 2-bar starter curve, 1–16 bar parameter,
Scale Lock, Key, Bend Range, HOST SYNC / NOTE trigger, loop, bypass, and
versioned VST3 component-state persistence.

## Scope limits (must not be described as complete)
- The custom VSTGUI editor and point editing paths have now been implemented
  in source, but remain uncompiled/unverified pending Windows CI and DAW tests.
- Timeline uses 4/4. Only target knots are scale-locked.
- Standard MIDI Pitch Bend is channel-wide; no MPE.
- Same-channel overlapping note triggers choose the latest note for Scale Lock.
- The scheduler samples continuous bend values at 64-sample intervals.
  Exact knot-boundary timing under arbitrary host block sizes must be tested.
- Pitch bend output requires a receiving host/instrument that accepts
  LegacyMIDICCOutEvent and matches the selected pitch-bend range.
- No build/validator/Studio One acceptance has yet been verified.
- No project-state mutation is accepted as a successful GUI edit until
  VSTGUI editor + controller-to-processor communication are implemented.

## Engineering evidence
- SDK 3.8.1 tag v3.8.1_build_84, with SDK-owned submodules.
- Explicit process-context requests: PPQ, tempo, transport.
- Processor returns kResultOk in setProcessing; AudioEffect default is
  kNotImplemented.
- 14-bit center=8192; LSB/MSB event encoding matches SDK.
- Changes are grouped on development branch v1.0.0.
- Core/scale and scheduler tests defined in tests/. Windows build unverified.
- No full end-to-end GUI/parameter/MIDI audit yet.

## Next required actions
1. Compile Windows x64 with MSVC against pinned SDK and inspect real CI logs.
2. Debug compile/validator and add direct MIDI event-list integration tests.
3. Validate the draft VSTGUI custom curve view in Studio One at 100/150%.
4. Finish numeric time/pitch point entry, GUI polish and graphical control audit.
5. Add Controller->Processor event/parameter protocol and complete lifecycle,
   automation, state and Studio One end-to-end tests.
6. Ship only after 125A release gates and user DAW acceptance.

## CI diagnostic 2026-10-12

First intentional Windows run [38251738035](https://github.com/challanger2000/125A-Pitchator/actions/runs/38251738035):
SDK configuration PASS. Native build failed because PitchatorVST.cpp called
UIAttributes::getAttributeValue while only a forward declaration was visible.
The implementation now explicitly includes the SDK-matched
`vstgui/uidescription/uiattributes.h`. This is a source correction;
Windows rebuild, validator, tests and DAW acceptance remain NOT VERIFIED.

## Verified follow-up — Build #5 (2026-10-12)

The original milestone text above is historical. The native VST3 now compiles
under pinned SDK 3.8.1 with MSVC Windows x64; **5/5 CTest** and **47/47
Steinberg Validator** tests pass. GitHub Actions run:
https://github.com/challanger2000/125A-Pitchator/actions/runs/38253823977

The first validator failure was BYPASS preset persistence on a zero-buffer
parameter-only flush, fixed in commit f2c91e5, directly regression-tested and
validated in Build #5. No Studio One or 125A Tester sign-off yet.
