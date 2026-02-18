# Animation and skinning roadmap (future task)

Status: planned, intentionally not implemented yet.

This document defines the target design for real glTF animation and skinning support.
Current runtime behavior logs reminders when animated/skinned data is detected, so the task is never forgotten.

---

## Scope

Implement full glTF 2.0 runtime support for:
- skeletal skinning (`skins`, `JOINTS_0`, `WEIGHTS_0`)
- animation clips (`animations` with samplers/channels)
- per-node animated transforms (TRS)

Out of scope for first iteration:
- morph targets animation
- retargeting across different skeletons
- animation blending trees

---

## Runtime design

### 1) Data import layer

- Parse and store from glTF:
  - nodes hierarchy
  - skins: joints, inverse bind matrices, skeleton root
  - animations: samplers (input/output), channels (target node/path)
- Convert to internal runtime structures with validated indices and contiguous memory.

### 2) Scene/runtime layer

- Add animation state per object/instance:
  - active clip
  - local time
  - playback rate
  - loop flag
- Evaluate animation each frame:
  - interpolate keyframes
  - update local TRS for animated nodes
  - rebuild global joint matrices

### 3) GPU layer

- Upload final joint matrices per draw/instance (UBO/SSBO).
- Vertex shader path for skinned meshes:
  - read joint indices/weights
  - blend joint matrices
  - output skinned position/normal

---

## Milestones

### M1 — Import only (no rendering changes)
- Parse and validate skin + animation data.
- Build internal clip/skeleton structures.
- Add debug dumps and validation logs.

### M2 — CPU evaluation
- Time update + keyframe interpolation.
- Node TRS animation and hierarchy update.
- Compute final joint matrices.

### M3 — GPU skinning
- Add skinned vertex format + shader variant.
- Upload joint matrices and render animated meshes.

### M4 — Editor/runtime controls
- Clip selection, play/pause, speed, loop.
- Debug visualizations (skeleton bones optional).

---

## Acceptance criteria

- Animated glTF files play correctly with expected timing.
- Skinned meshes deform correctly (no exploding vertices).
- Multiple animated objects can run in the same scene.
- Logs are clean in normal runs; warnings only for unsupported features.

---

## Current placeholder hooks

Animation/skinning reminders currently live in scene loading stubs:
- `PrepareAnimationImportStub()`
- `PrepareSkinningImportStub()`

Those stubs print warnings when `animations`, `skins`, `JOINTS_0`, or `WEIGHTS_0` are detected.
