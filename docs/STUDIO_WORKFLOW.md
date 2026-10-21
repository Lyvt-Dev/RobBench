# Studio installation and end-to-end checks

**Runtime status: not executed in Roblox Studio in this development environment.**
The plugin passes Luau syntax compilation. Its API choices were checked against the official Creator Docs.
Follow these reproducible checks on Windows or macOS before treating Roblox playback as accepted.

## Install

1. In an empty Studio place, create a Script and replace its source with
   `plugins/roblox-studio/RobBench.plugin.luau`.
2. Right-click the Script and choose **Save as Local Plugin**. Name it RobBench.
   Remove the source Script from the place afterward so it cannot execute as a game script.
3. Enable the local plugin if necessary, and click its RobBench toolbar button.
   Local file import uses StudioService:PromptImportFileAsync and File:GetBinaryContents;
   no web endpoint, HTTP permission, cookie or API key is used by the animation importer.

## Fast template round trip

1. In RobBench, create R15, select LeftUpperArm, insert a rest key at 0.
2. At 1 second set Rotation X to 70 degrees. At 2 seconds restore 0.
3. Save `arm.robbench`, reopen it and play it. Export `arm.rbanim` at 60 FPS.
4. In Studio, click **Create matching block rig from .rbanim**, and select the file.
   This builds the exact exported block geometry and Motor6D pivots, with an AnimationController.
   It creates a KeyframeSequence in the model's RobBenchAnimations folder.
5. Inspect the sequence: times start at 0 and end at 2; the LeftUpperArm pose is under UpperTorso,
   under LowerTorso, under HumanoidRootPart. The 1-second X rotation should be about 70 degrees.
6. Start **Run**. Select the copied sequence on the active simulation model and click **Preview**.
   Verify the left arm moves, connected lower arm/hand follow, the right arm remains still,
   and motion repeats at two seconds. Stop preview and stop simulation.
7. Repeat with R6. Its arm pose is named `Left Arm`, parented under `Torso`; the motor is
   `Left Shoulder`. R6 and R15 exports must not be interchangeable.

Generated rigs are anchored at HumanoidRootPart, contain non-colliding block parts, and use
AnimationController rather than Humanoid locomotion. They are authoring/verification rigs, not avatar assets.

## Existing avatar / exact rig workflow

1. Select an existing character Model with HumanoidRootPart and a connected Motor6D tree.
   Alternatively enter a username or numeric ID and click **Import avatar into Studio**.
   The latter uses Players:GetUserIdFromNameAsync and Players:CreateHumanoidModelFromUserIdAsync.
   Roblox determines source R6/R15, asset availability and access; API failures appear in the plugin.
2. Click **Export selected rig → JSON below**. Copy all JSON and save UTF-8 text as `avatar.robbench`.
3. Open that file in RobBench. Body parts are **bounding boxes**; avatar meshes, accessories,
   clothing, textures and layered clothing are not transferred to the native editor.
   Exact sizes, C0/C1 orientations, pivots, motor names and hierarchy are transferred.
4. Animate, save and export `.rbanim`.
5. Select the original Studio character, click **Import .rbanim onto selected rig**.
   The plugin validates every mapped joint, hierarchy and pivot before adding the sequence.
6. Start Run/Play, select the active model's imported sequence, and Preview. Ensure no other
   animation of equal/higher priority masks the result. Preview uses Animator:LoadAnimation and
   KeyframeSequenceProvider:RegisterKeyframeSequence, which gives a temporary **local Studio** ID.
7. To use it in a published game, use Roblox Studio's supported animation publishing workflow
   with the appropriate creator/group permissions. This plugin does not upload or publish assets.

## Negative checks

- Import R15 data onto R6: a clear rig type or part mismatch must be reported.
- Change one motor C0/C1: import must reject it and recommend re-exporting the rig.
- Rename a joint, remove a limb, duplicate motor names or introduce a cycle: reject before creation.
- Corrupt a quaternion, timestamp or version: reject without leaving a partial sequence.
- Try scaled animation or World-root translation: native export must reject it.
- Ctrl+Z after an import/create-rig should remove that import as a single Studio history operation.
- Try an invalid/private/unavailable avatar ID: report the API error without pretending it imported.
- Verify animation duration, loop state, every keyed joint, and off-frame timestamps.

Acceptance evidence should include Studio version, OS, rig source, export FPS, a screenshot/video,
expected versus observed transforms, errors, and both R6 and R15 results. This evidence is still outstanding.
