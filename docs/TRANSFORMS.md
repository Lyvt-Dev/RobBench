# Direct viewport transforms

RobBench 0.1.0 provides interactive, axis-constrained handles in the native Vulkan viewport.
Select a part first, then choose a tool from the toolbar:

| Tool | Shortcut | Result |
|---|---|---|
| Select | Toolbar | Object picking without transform handles |
| Move | W | Translate the selected joint along one axis; commit a position key at the current time |
| Rotate | E | Drag an axis ring around the Motor6D pivot; commit a quaternion rotation key |
| Resize | R | Resize the block geometry along a part-local axis; no animation scale track is created |

Use **Joint local / World** to choose move/rotation axes. Resize always uses **Part local**.
The red, green and blue handles correspond to X, Y and Z. Hover/drag highlights the active axis.
Handles remain a consistent screen size as the camera zooms and are rendered as a Vulkan overlay.

Hold Shift during a drag or enable **Snap** for 0.1-stud movement/size increments and 15-degree rotations.
Movement snapping rounds the displacement from the drag start; size snapping rounds the resulting dimension.
Release the left mouse button to commit exactly one undo command. Esc, focus loss, changing time/selection
or changing tools cancels the preview. Clicking a handle without moving it creates no key or history entry.
Playback pauses when a handle drag begins. W/E/R shortcuts do not activate while typing in editor inputs.

Insert a rest-pose key before posing a later time. The current easing selector is applied to a newly committed key.
Undo/redo covers a whole drag, not every mouse movement. The inspector updates live during the preview,
but autosave/export see the committed document until the drag finishes.

## Motor6D semantics

The joint frame is `ParentWorld * C0 * AnimatedTransform`, equivalently `PartWorld * C1`.
Move/rotate handles sit at this frame's origin. Local axes include the current joint rotation.
World movement is converted through `inverse(ParentWorld * C0)` before updating the animated translation.
World rotation is conjugated through the parent joint basis before updating its quaternion.
Children continue to inherit the result through the Motor6D hierarchy; they are not independently repositioned.
Tests cover rotated C0/C1 and animated ancestors, not just identity templates.

Resize handles instead sit at the block center, with axes from its full part-world orientation, including C1.
Resizing changes `Part.size`, so joint pivots and child attachment positions remain fixed. It is not avatar
proportion retargeting and does not implement Roblox animation scaling. Dimensions are clamped to 0.01–1000 studs.

Handles are unavailable for locked parts and for hierarchies containing non-unit joint scale. Such imported
scale needs a future dedicated rig-scaling/retargeting workflow. World-root animation remains unsupported in
Roblox interchange; animate the Root/RootJoint motor for body motion, as documented in FORMATS.md.

## Rendering quality

4x MSAA is enabled by default when supported by the selected Vulkan device; otherwise rendering uses one sample.
View → **4x anti-aliasing (next launch)** persists the preference. Changing it takes effect on the next launch.
This adds hardware-supported multisample rasterization; it does not imply shadows, texture painting or measured 60 FPS.
