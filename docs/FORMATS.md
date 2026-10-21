# Version 1 formats

Both formats are UTF-8 JSON, case-sensitive and independently versioned. Unknown versions are rejected.
Units are Roblox studs and seconds. Coordinates are right-handed, +Y up, forward -Z. Matrices use
column vectors. JSON quaternion order is **[x,y,z,w]**; internal GLM construction is `(w,x,y,z)`.
Quaternions must be normalized. Euler XYZ controls are converted to quaternions; no Euler component
interpolation is implied. There is no graph editor in version 0.1.

## .robbench

Top-level keys: `format: "robbench"`, `version: 1`, `name`, `rigType`, `parts`, `clips`.
`rigType` is `R6`, `R15` or a custom label. Each part has:

```json
{
  "name": "LeftUpperArm", "joint": "LeftShoulder", "parent": 2,
  "size": [1,1.1,1],
  "c0": {"position":[-1,0.7,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
  "c1": {"position":[0.5,0.45,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
  "color": [0.83,0.71,0.52], "visible":true, "locked":false
}
```

`parent` is a zero-based array index; `-1` is the single root. Array order need not be topological.
Names and joint identifiers must be unique. Cycles, unknown parents, zero/negative dimensions, invalid
rotations and non-finite numbers are rejected. Root `joint: "World"` is an editor track, not a Motor6D.
Pivots C0/C1 are rigid transforms; their scale must be one.

The defining Motor6D relation is:

```
WorldChild * C1 = WorldParent * C0 * AnimatedTransform
WorldChild = WorldParent * C0 * AnimatedTransform * inverse(C1)
```

Root world transform is `C0 * AnimatedTransform * inverse(C1)`.
Size scales only the rendered unit cube, not the child's joint hierarchy.

Each clip has `name`, `duration` (>0), `loop`, `tracks`. Each track has `joint` and sorted `keys`.
Each key has `time`, `value` (position/rotation/scale transform), and `interpolation` integer:
0 Step, 1 Linear, 2 quadratic EaseIn, 3 quadratic EaseOut, 4 smoothstep EaseInOut.
Interpolation belongs to the **outgoing** interval. Translation/scale use component interpolation;
rotation uses normalized shortest-path quaternion SLERP with the eased time parameter.
Outside keyed ranges, endpoint poses hold. Evaluation never implicitly wraps; playback wraps loop time.

Save uses QSaveFile atomic commit. Reads are limited to 64 MiB, 10,000 parts, 1,000 clips and
100,000 keys per track. This initial format contains block geometry and joint animation only.
Mesh topology, image texture data, UVs, materials, events, and per-project workspace data are future schema additions.
Application layout/settings live separately in QSettings.

## .rbanim

Top-level keys: `format: "rbanim"`, `version: 1`, `name`, `rigType`, `duration`, `loop`,
`fps`, `interpolation: "baked-linear"`, `parts` (same rig records), `frames`.
A frame has `time` and `transforms`, a same-length array corresponding **by index** to `parts`.

Export samples at the chosen 24/30/60 FPS (core API supports 1–240), adds every original key time,
and always includes 0 and exact duration. Nonlinear easing and SLERP are evaluated in the native engine
before baking. Steps include a pre-transition sample 10 microseconds before the key where possible;
the finite baked transition is an approximation, explicitly not lossless constant interpolation.
Baked linear quaternion playback approximates nonlinear native motion between samples. Raise FPS to reduce error.
No source key time is silently dropped. Native source curves remain in the saved project.

Scale animation and non-identity World-root tracks are rejected before export. Animate the rig's
Root or RootJoint Motor6D for root-body motion. Moving HumanoidRootPart through world space requires
separate Roblox gameplay/root-motion handling and is not encoded by the current interchange.
Exports are capped at 500,000 part transforms; importer caps additionally limit parts to 1,000.
The desktop importer reconstructs baked linear tracks; it does not recover original source curves.

Studio requires identical part names, joint names, hierarchy and C0/C1 pivots. Pose hierarchy uses
**body part names**, not motor names, with each child pose under its Part0 pose. Each Pose.CFrame is
the exported AnimatedTransform. Import does not overwrite C0/C1 or approximate incompatible pivots.
No automatic retargeting, scale conversion, animation uploading or publication is implemented.
