-- RobBench companion plugin. MIT License; see repository LICENSE.
-- Install as a local Studio plugin. No HTTP permission or external credentials required.
local HttpService = game:GetService("HttpService")
local Selection = game:GetService("Selection")
local StudioService = game:GetService("StudioService")
local Players = game:GetService("Players")
local ChangeHistoryService = game:GetService("ChangeHistoryService")
local KeyframeSequenceProvider = game:GetService("KeyframeSequenceProvider")
local toolbar = plugin:CreateToolbar("RobBench")
local toggle = toolbar:CreateButton("RobBench", "Import animations and export exact rig pivots", "")
local info = DockWidgetPluginGuiInfo.new(Enum.InitialDockState.Right, false, false, 420, 650, 320, 400)
local widget = plugin:CreateDockWidgetPluginGui("RobBenchCompanionV1", info)
widget.Title = "RobBench"
local body = Instance.new("Frame")
body.Size = UDim2.fromScale(1, 1)
body.BackgroundColor3 = Color3.fromRGB(37, 40, 48)
body.Parent = widget
local layout = Instance.new("UIListLayout")
layout.Padding = UDim.new(0, 8)
layout.Parent = body
local padding = Instance.new("UIPadding")
padding.PaddingLeft = UDim.new(0, 10)
padding.PaddingRight = UDim.new(0, 10)
padding.PaddingTop = UDim.new(0, 10)
padding.Parent = body
local function label(text, height)
    local item = Instance.new("TextLabel")
    item.Size = UDim2.new(1, 0, 0, height or 44)
    item.BackgroundTransparency = 1
    item.TextColor3 = Color3.fromRGB(220, 220, 230)
    item.TextWrapped = true
    item.TextSize = 14
    item.Text = text
    item.Parent = body
    return item
end
label("Select a character Model. Export its rig to preserve exact Motor6D pivots before animating.", 54)
local status = label("Ready", 70)
local function button(text)
    local item = Instance.new("TextButton")
    item.Size = UDim2.new(1, 0, 0, 30)
    item.BackgroundColor3 = Color3.fromRGB(102, 77, 149)
    item.TextColor3 = Color3.new(1, 1, 1)
    item.TextSize = 14
    item.Text = text
    item.Parent = body
    return item
end
local function input(placeholder, height)
    local box = Instance.new("TextBox")
    box.Size = UDim2.new(1, 0, 0, height)
    box.BackgroundColor3 = Color3.fromRGB(27, 30, 36)
    box.TextColor3 = Color3.new(1, 1, 1)
    box.PlaceholderText = placeholder
    box.Text = ""
    box.TextSize = 13
    box.ClearTextOnFocus = false
    box.Parent = body
    return box
end
local username = input("Roblox username or numeric user ID", 28)
local loadAvatar = button("Import avatar into Studio")
local exportRig = button("Export selected rig → JSON below")
local importAnimation = button("Import .rbanim onto selected rig")
local createRig = button("Create matching block rig from .rbanim")
local preview = button("Preview imported animation (Play/Run mode)")
local stop = button("Stop preview")
label("Rig JSON: Ctrl+A, Ctrl+C; save as a UTF-8 .robbench file, then open in RobBench.", 44)
local output = input("Rig JSON appears here", 120)
output.MultiLine = true
output.TextXAlignment = Enum.TextXAlignment.Left
output.TextYAlignment = Enum.TextYAlignment.Top
output.TextWrapped = false
local lastSequence, lastModel, previewTrack, previewAnimation
local busy = false
local function guarded(fn)
    return function()
        if busy then return end
        busy = true
        task.spawn(function()
            local ok, problem = pcall(fn)
            busy = false
            if not ok then
                status.Text = tostring(problem)
                warn("RobBench: " .. tostring(problem))
            end
        end)
    end
end
local function selectedModel()
    local selected = Selection:Get()
    assert(#selected == 1 and selected[1]:IsA("Model"), "Select exactly one character Model in Explorer.")
    local model = selected[1]
    assert(model:IsDescendantOf(workspace), "Character must be in Workspace.")
    return model
end
local function finite(v)
    return type(v) == "number" and v == v and math.abs(v) < 1e9
end
local function vector(v)
    assert(type(v) == "table" and #v == 3, "Invalid three-vector")
    for _, n in ipairs(v) do assert(finite(n), "Non-finite vector component") end
    return Vector3.new(v[1], v[2], v[3])
end
local function cframe(t)
    assert(type(t) == "table", "Missing transform")
    local position = vector(t.position)
    local scale = vector(t.scale)
    assert((scale - Vector3.one).Magnitude < 1e-5, "Motor6D scale is unsupported")
    local q = t.rotation
    assert(type(q) == "table" and #q == 4, "Expected XYZW quaternion")
    local norm = 0
    for _, n in ipairs(q) do assert(finite(n), "Invalid quaternion"); norm += n * n end
    assert(math.abs(norm - 1) < 0.002, "Quaternion must have unit length")
    local x, y, z, w = q[1], q[2], q[3], q[4]
    return CFrame.new(position.X, position.Y, position.Z,
        1 - 2 * (y*y + z*z), 2 * (x*y - z*w), 2 * (x*z + y*w),
        2 * (x*y + z*w), 1 - 2 * (x*x + z*z), 2 * (y*z - x*w),
        2 * (x*z - y*w), 2 * (y*z + x*w), 1 - 2 * (x*x + y*y))
end
local function identity(cf)
    local _, angle = cf:ToAxisAngle()
    return cf.Position.Magnitude < 1e-4 and math.abs(angle) < 1e-4
end
local function equal(a, b)
    return identity(a:Inverse() * b)
end
local function transform(cf)
    local axis, angle = cf:ToAxisAngle()
    local s = math.sin(angle / 2)
    return {position = {cf.X, cf.Y, cf.Z}, rotation = {axis.X*s, axis.Y*s, axis.Z*s, math.cos(angle/2)}, scale = {1, 1, 1}}
end
local function vec(v) return {v.X, v.Y, v.Z} end
local function rigData(model)
    local root = model:FindFirstChild("HumanoidRootPart")
    assert(root and root:IsA("BasePart"), "Rig must contain HumanoidRootPart")
    local motors, byChild, names = {}, {}, {}
    for _, item in ipairs(model:GetDescendants()) do
        if item:IsA("Motor6D") then
            assert(item.Part0 and item.Part1, "Disconnected Motor6D: " .. item.Name)
            assert(item.Part0:IsDescendantOf(model) and item.Part1:IsDescendantOf(model), "Motor references an external part")
            assert(not names[item.Name], "Duplicate Motor6D name: " .. item.Name)
            assert(not byChild[item.Part1], "A body part has more than one parent motor")
            names[item.Name] = true
            byChild[item.Part1] = item
            table.insert(motors, item)
        end
    end
    assert(not byChild[root], "HumanoidRootPart must be the rig root")
    local ordered, indices = {root}, {[root] = 0}
    local incoming = {}
    local cursor = 1
    while cursor <= #ordered do
        local parent = ordered[cursor]
        for _, motor in ipairs(motors) do
            if motor.Part0 == parent then
                assert(indices[motor.Part1] == nil, "Cyclic rig")
                indices[motor.Part1] = #ordered
                table.insert(ordered, motor.Part1)
                incoming[motor.Part1] = motor
            end
        end
        cursor += 1
    end
    assert(#ordered == #motors + 1, "Disconnected or cyclic Motor6D hierarchy")
    local parts, partNames = {}, {}
    for _, part in ipairs(ordered) do
        assert(not partNames[part.Name], "Duplicate body part name: " .. part.Name)
        partNames[part.Name] = true
        local motor = incoming[part]
        table.insert(parts, {
            name = part.Name, joint = motor and motor.Name or "World",
            parent = motor and indices[motor.Part0] or -1,
            size = vec(part.Size), c0 = transform(motor and motor.C0 or CFrame.new(0, 3, 0)),
            c1 = transform(motor and motor.C1 or CFrame.identity),
            color = {part.Color.R, part.Color.G, part.Color.B},
            visible = part ~= root and part.Transparency < 1, locked = false,
        })
    end
    return parts, incoming, ordered
end
loadAvatar.MouseButton1Click:Connect(guarded(function()
    local value = username.Text:match("^%s*(.-)%s*$")
    assert(value ~= "", "Enter a username or user ID")
    local userId = tonumber(value)
    if not userId then userId = Players:GetUserIdFromNameAsync(value) end
    assert(userId > 0 and userId % 1 == 0, "Invalid user ID")
    local model = Players:CreateHumanoidModelFromUserIdAsync(userId)
    local recording = ChangeHistoryService:TryBeginRecording("RobBenchImportAvatar", "Import avatar")
    assert(recording, "Studio is busy; try again")
    model.Parent = workspace
    model:PivotTo(CFrame.new(0, 4, 0))
    ChangeHistoryService:FinishRecording(recording, Enum.FinishRecordingOperation.Commit)
    Selection:Set({model})
    status.Text = "Avatar imported through Roblox Players API. Export rig to author its joint animation. Meshes/textures remain in Studio."
end))
exportRig.MouseButton1Click:Connect(guarded(function()
    local model = selectedModel()
    local parts = rigData(model)
    local humanoid = model:FindFirstChildOfClass("Humanoid")
    local rigType = humanoid and humanoid.RigType.Name or "Custom"
    output.Text = HttpService:JSONEncode({format = "robbench", version = 1, name = model.Name, rigType = rigType,
        parts = parts, clips = {{name = "Animation", duration = 2, loop = true, tracks = {}}}})
    output:CaptureFocus()
    output.SelectionStart = 1
    output.CursorPosition = #output.Text + 1
    status.Text = "Exact C0/C1 pivots exported. Geometry is represented by part bounding boxes. Copy JSON and save as .robbench."
end))
local function buildSequence(data, model)
    assert(data.format == "rbanim" and data.version == 1, "Unsupported .rbanim version")
    assert(data.interpolation == "baked-linear", "Expected baked-linear interpolation")
    assert(finite(data.duration) and data.duration > 0, "Invalid animation duration")
    assert(type(data.loop) == "boolean" and type(data.name) == "string", "Invalid clip metadata")
    assert(type(data.parts) == "table" and #data.parts > 0 and #data.parts <= 1000, "Invalid rig size")
    assert(type(data.frames) == "table" and #data.frames >= 2 and #data.frames <= 200000, "Invalid frame count")
    assert(#data.frames * #data.parts <= 500000, "Animation too large for this importer; shorten it or lower export FPS")
    local targetParts, incoming, ordered = rigData(model)
    local targetByName = {}
    for i, part in ipairs(ordered) do targetByName[part.Name] = {part = part, data = targetParts[i], motor = incoming[part]} end
    local humanoid = model:FindFirstChildOfClass("Humanoid")
    if humanoid and (data.rigType == "R6" or data.rigType == "R15") then
        assert(humanoid.RigType.Name == data.rigType, "R6/R15 rig type mismatch")
    end
    local roots, names, joints = 0, {}, {}
    local mismatchedPivots = false
    for i, part in ipairs(data.parts) do
        assert(type(part.name) == "string" and not names[part.name], "Duplicate or invalid part name")
        assert(type(part.joint) == "string" and not joints[part.joint], "Duplicate or invalid joint name")
        names[part.name], joints[part.joint] = true, true
        assert(type(part.parent) == "number" and part.parent % 1 == 0 and part.parent >= -1 and part.parent < #data.parts, "Invalid parent index")
        local target = targetByName[part.name]
        assert(target, "Target is missing body part " .. part.name)
        if part.parent == -1 then
            roots += 1
            assert(target.part.Name == "HumanoidRootPart", "Root must be HumanoidRootPart")
        else
            local motor = target.motor
            assert(motor and motor.Name == part.joint, "Motor6D mismatch for " .. part.name)
            assert(motor.Part0.Name == data.parts[part.parent + 1].name, "Parent mismatch for " .. part.name)
            if not equal(cframe(part.c0), motor.C0) or not equal(cframe(part.c1), motor.C1) then mismatchedPivots = true end
        end
        local seen, index = {}, i
        while index ~= 0 do
            assert(not seen[index], "Cyclic animation hierarchy")
            seen[index] = true
            index = data.parts[index].parent + 1
        end
    end
    assert(roots == 1 and #data.parts == #targetParts, "Source/target joint counts differ")
    assert(not mismatchedPivots,
        "Joint pivots differ. Export this exact Studio rig to .robbench, animate it, then re-export. Automatic retargeting is not implemented.")
    local lastTime = -1
    for _, frame in ipairs(data.frames) do
        assert(finite(frame.time) and frame.time > lastTime and frame.time >= 0 and frame.time <= data.duration, "Invalid or unsorted frame time")
        assert(type(frame.transforms) == "table" and #frame.transforms == #data.parts, "Frame does not match rig")
        for i, value in ipairs(frame.transforms) do
            local cf = cframe(value)
            if data.parts[i].parent == -1 then assert(identity(cf), "World-root animation is unsupported; animate Root/RootJoint instead") end
        end
        lastTime = frame.time
    end
    assert(data.frames[1].time == 0 and math.abs(lastTime - data.duration) < 1e-6, "Export must include clip endpoints")
    local sequence = Instance.new("KeyframeSequence")
    sequence.Name = data.name
    sequence.Loop = data.loop
    sequence.Priority = Enum.AnimationPriority.Action
    local ok, problem = pcall(function()
        for _, frame in ipairs(data.frames) do
            local keyframe = Instance.new("Keyframe")
            keyframe.Time = frame.time
            keyframe.Parent = sequence
            local poses = {}
            for i, part in ipairs(data.parts) do
                local pose = Instance.new("Pose")
                pose.Name = part.name
                pose.CFrame = cframe(frame.transforms[i])
                pose.Weight = 1
                pose.EasingStyle = Enum.PoseEasingStyle.Linear
                pose.EasingDirection = Enum.PoseEasingDirection.InOut
                poses[i] = pose
            end
            for i, part in ipairs(data.parts) do
                poses[i].Parent = part.parent == -1 and keyframe or poses[part.parent + 1]
            end
        end
    end)
    if not ok then sequence:Destroy(); error(problem) end
    return sequence
end
importAnimation.MouseButton1Click:Connect(guarded(function()
    local model = selectedModel()
    local file = StudioService:PromptImportFileAsync({"rbanim"})
    if not file then return end
    assert(file.Size <= 64 * 1024 * 1024, "File exceeds 64 MiB")
    local data = HttpService:JSONDecode(file:GetBinaryContents())
    local sequence = buildSequence(data, model)
    local recording = ChangeHistoryService:TryBeginRecording("RobBenchImportAnimation", "Import RobBench animation")
    if not recording then sequence:Destroy(); error("Studio is busy; try again") end
    local folder = model:FindFirstChild("RobBenchAnimations")
    if not folder then folder = Instance.new("Folder"); folder.Name = "RobBenchAnimations"; folder.Parent = model end
    sequence.Parent = folder
    ChangeHistoryService:FinishRecording(recording, Enum.FinishRecordingOperation.Commit)
    lastSequence, lastModel = sequence, model
    Selection:Set({sequence})
    status.Text = "Imported " .. #data.frames .. " baked frames with exact pivots. In Play/Run mode, Preview uses a temporary local animation ID. Publish separately in Studio."
end))
createRig.MouseButton1Click:Connect(guarded(function()
    local file = StudioService:PromptImportFileAsync({"rbanim"})
    if not file then return end
    assert(file.Size <= 64 * 1024 * 1024, "File exceeds 64 MiB")
    local data = HttpService:JSONDecode(file:GetBinaryContents())
    assert(data.format == "rbanim" and data.version == 1, "Unsupported animation format")
    assert(type(data.parts) == "table" and #data.parts > 0 and #data.parts <= 1000, "Invalid rig size")
    local model = Instance.new("Model")
    model.Name = "RobBench_" .. tostring(data.rigType)
    local ok, sequence = pcall(function()
        local instances, worlds, visiting = {}, {}, {}
        for i, part in ipairs(data.parts) do
            assert(type(part.name) == "string", "Invalid part name")
            local instance = Instance.new("Part")
            instance.Name = part.name
            local size = vector(part.size)
            assert(size.X > 0 and size.Y > 0 and size.Z > 0, "Invalid part size")
            instance.Size = size
            local color = vector(part.color)
            instance.Color = Color3.new(color.X, color.Y, color.Z)
            instance.CanCollide = false
            instance.Massless = part.parent ~= -1
            instance.Anchored = part.parent == -1
            instance.Transparency = part.visible and 0 or 1
            instance.Parent = model
            instances[i] = instance
        end
        local function world(i)
            assert(i >= 1 and i <= #data.parts and not visiting[i], "Invalid or cyclic hierarchy")
            if worlds[i] then return worlds[i] end
            visiting[i] = true
            local part = data.parts[i]
            assert(type(part.parent) == "number" and part.parent % 1 == 0 and part.parent >= -1 and part.parent < #data.parts, "Invalid parent")
            local localFrame = cframe(part.c0) * cframe(part.c1):Inverse()
            worlds[i] = part.parent == -1 and localFrame or world(part.parent + 1) * localFrame
            visiting[i] = nil
            return worlds[i]
        end
        for i, part in ipairs(data.parts) do
            instances[i].CFrame = world(i)
            if part.parent ~= -1 then
                local motor = Instance.new("Motor6D")
                motor.Name = part.joint
                motor.Part0 = instances[part.parent + 1]
                motor.Part1 = instances[i]
                motor.C0 = cframe(part.c0)
                motor.C1 = cframe(part.c1)
                motor.Parent = instances[i]
            else
                model.PrimaryPart = instances[i]
            end
        end
        local controller = Instance.new("AnimationController")
        controller.Parent = model
        Instance.new("Animator").Parent = controller
        return buildSequence(data, model)
    end)
    if not ok then model:Destroy(); error(sequence) end
    local recording = ChangeHistoryService:TryBeginRecording("RobBenchCreateRig", "Create RobBench block rig")
    if not recording then model:Destroy(); sequence:Destroy(); error("Studio is busy; try again") end
    local folder = Instance.new("Folder")
    folder.Name = "RobBenchAnimations"
    folder.Parent = model
    sequence.Parent = folder
    model.Parent = workspace
    ChangeHistoryService:FinishRecording(recording, Enum.FinishRecordingOperation.Commit)
    lastSequence, lastModel = sequence, model
    Selection:Set({sequence})
    status.Text = "Created an AnimationController block rig with exact source pivots and imported animation. Preview in Run mode. This is an authoring rig, not an avatar mesh."
end))
local function stopPreview()
    if previewTrack then previewTrack:Stop(0); previewTrack:Destroy(); previewTrack = nil end
    if previewAnimation then previewAnimation:Destroy(); previewAnimation = nil end
end
preview.MouseButton1Click:Connect(guarded(function()
    assert(game:GetService("RunService"):IsRunning(), "Start Studio Play or Run mode before previewing")
    -- Selecting an imported sequence also allows preview after a Play/Run data-model switch.
    local selected = Selection:Get()
    if #selected == 1 and selected[1]:IsA("KeyframeSequence") then
        lastSequence = selected[1]
        lastModel = lastSequence:FindFirstAncestorOfClass("Model")
    end
    assert(lastSequence and lastSequence.Parent and lastModel and lastModel.Parent, "Select an imported KeyframeSequence on the active simulation rig")
    stopPreview()
    local controller = lastModel:FindFirstChildOfClass("Humanoid") or lastModel:FindFirstChildOfClass("AnimationController")
    assert(controller, "Target needs Humanoid or AnimationController")
    local animator = controller:FindFirstChildOfClass("Animator")
    if not animator then animator = Instance.new("Animator"); animator.Parent = controller end
    previewAnimation = Instance.new("Animation")
    previewAnimation.AnimationId = KeyframeSequenceProvider:RegisterKeyframeSequence(lastSequence)
    previewTrack = animator:LoadAnimation(previewAnimation)
    previewTrack:Play(0)
    status.Text = "Preview playing. Temporary animation IDs work only in this Studio session."
end))
stop.MouseButton1Click:Connect(stopPreview)
toggle.Click:Connect(function() widget.Enabled = not widget.Enabled end)
plugin.Unloading:Connect(stopPreview)
