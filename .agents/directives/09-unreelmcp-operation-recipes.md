# 09. UnreelMCP Operation Recipes Directive for AI Agents

This directive contains domain-specific UnreelMCP operation recipes. Read it only after [`08-unreelmcp-tools.md`](08-unreelmcp-tools.md) confirms that a task needs live Editor or asset operations.

## General Recipe Rules

- Inspect before and after every graph, asset, or world mutation.
- Keep returned node ids, GUIDs, indexes, or object names for follow-up calls instead of guessing identifiers.
- Save assets after successful mutations when the operation does not save automatically.
- Treat schema rejection, unsupported operations, and property suggestions as useful feedback instead of retrying the same call blindly.

## Audio, Foliage, And Landscape

- For SoundCue graph work, inspect before and after every graph mutation. Use the node indexes returned by `audio.soundcue.inspect` for `audio.soundcue.connect`, `audio.soundcue.disconnect`, `audio.soundcue.set_output`, `audio.soundcue.set_node_property`, and `audio.soundcue.remove_node`, then save the asset.
- For MetaSound Source graph work, inspect before and after every graph mutation. Use node GUIDs and named pins from `audio.metasound.inspect` for `audio.metasound.connect`, `audio.metasound.disconnect_input`, `audio.metasound.set_node_input_default`, and `audio.metasound.remove_node`, then save the asset.
- For foliage work, create a foliage type with `foliage.type_create`, place transforms with `foliage.add_instance` or `foliage.add_instances`, and verify with `foliage.inspect`. Use `foliage.remove_instances` only with an explicit selector (`indices`, `sphere`, `box`, or `all=true`) and inspect before index-based removal because deletion can swap instance order.
- For Landscape work, use `landscape.create` for flat actor creation, `landscape.height.set_region` for rectangular sculpt edits, `landscape.layer.paint_region` for layer weights, and `landscape.inspect` to verify extents, component counts, material, scale, and target layers.

## Blueprint And Widget Assets

- For Blueprint graph edits, use `blueprint.graph.list` to capture node ids and member variables, then prefer dedicated operations. Use `blueprint.graph.add_node_by_class` only for concrete `UEdGraphNode`/`UK2Node` classes and always follow with compile/save verification.
- For common Blueprint nodes, prefer `blueprint.graph.add_common_node` over raw class spawning. It supports `branch`, `sequence`, `dynamic_cast`, `select`, `timeline`, `add_component_by_class`, `add_delegate`, `assign_delegate`, and `macro_instance`.
- For Blueprint pin defaults, use `blueprint.graph.set_pin_default`; Unreal's graph schema may reject invalid defaults, and that rejection should be reported instead of retried blindly.
- Use `default_value` for scalar/string pins, `default_object` for object/class pins, and `default_text` for FText pins.
- For Widget Blueprint tree edits, create the widget asset with `widget.create`, add a root panel using `widget.tree.add_widget` with `set_root=true`, add children by `parent` name, set widget properties with `widget.tree.set_property`, set slot layout properties with `widget.tree.set_slot_property`, verify with `widget.tree.list`, then `asset.save`.
- For Widget event/binding/MVVM work, use `widget.event.list` before `widget.event.bind`, verify with `widget.event.list`, use `widget.binding.add` with exactly one of `source_property` or `function`, and use `widget.mvvm.add_viewmodel` only with classes implementing `NotifyFieldValueChanged`.

## Animation Assets

- For Animation Blueprint authoring, create with `animation.anim_blueprint_create`, use normal Blueprint operations for member variables, verify graphs and variables with `animation.anim_blueprint.inspect`, and use `animation.anim_graph.inspect` for AnimGraph-specific node/state-machine/pose-link context before compile/save.
- For practical AnimGraph authoring, use `animation.anim_graph.add_node_by_class` with concrete public `UAnimGraphNode_Base` classes, `animation.anim_graph.set_node_animation_asset` for sequence/evaluator players, `animation.anim_graph.set_node_property` for reflected fields, and `animation.anim_graph.connect_pose_pins` for pose links. Always compile/save and verify with `animation.anim_graph.inspect`.
- For any Animation Blueprint blend node work, first inspect a known-working local blend as the gold standard and match its policy unless the user explicitly requests a different blend behavior. For `Layered Blend per Bone`, verify both the internal `blend_weights` array and the exposed `BlendWeights_*` pin default; a connected blend pose with weight `0.0` is effectively invisible. For full branch override, prefer `BlendDepth=0`, not a large depth value: in UE 5.8 nonzero depth distributes weight as `(Depth + 1) / BlendDepth`, so values such as `255` can make neck/head/face motion nearly invisible. Keep `CurveBlendOption=Override` and mesh-space rotation/scale blending off when that is what the working reference uses, then compile/save and re-inspect the resolved node struct.
- For Animation State Machine authoring, use `animation.state_machine.add`, `animation.state_machine.add_state`, `animation.state_machine.add_alias`, `animation.state_machine.set_alias`, `animation.state_machine.connect`, and `animation.state_machine.set_node_property`; verify states, State Alias target lists, entry links, transitions, and bound transition graphs with `animation.state_machine.inspect`.
- For Animation metadata, use `animation.metadata.add` with a concrete `UAnimMetaData` class. `/Script/UnreelMCP.UnreelMCPAnimMetaData` is available for simple `Tag`/`Value` metadata; project-specific metadata classes can be used by class path. Verify with `animation.metadata.list`, edit with `animation.metadata.set_property`, remove with `animation.metadata.remove`, then save the animation asset.
- For Animation Sequence and Montage notifies, inspect first with `animation.notify.list`, add name-only or `UAnimNotify` events with `animation.notify.add`, add `UAnimNotifyState` events with `animation.notify.add_state`, edit time/duration/track with `animation.notify.move`, edit reflected event or notify-object fields with `animation.notify.set_property`, remove with `animation.notify.remove`, and then save. Keep notify times and state durations inside the asset play length; generated one-frame assets may only allow time `0.0` and very short durations.
- For Montage structure edits, inspect with `animation.montage.inspect`, edit slots with `animation.montage.add_slot`/`animation.montage.remove_slot`, edit sections and `next_section` links with `animation.montage.add_section`/`animation.montage.set_section`/`animation.montage.remove_section`, and edit slot animation segments with `animation.montage.add_segment`/`animation.montage.set_segment`/`animation.montage.remove_segment`. Segment `animation_asset` values must be composable animation assets, not montages.

## AI Assets

- For Behavior Tree work, create a Blackboard first with `blackboard.create`, add keys with `blackboard.add_key`, then create the tree with `behavior_tree.create` and the `blackboard` parameter. Use `behavior_tree.inspect` after every structural edit and keep returned node ids for later calls. Use `behavior_tree.add_composite`, `behavior_tree.add_task`, `behavior_tree.add_wait`, `behavior_tree.add_decorator`, `behavior_tree.add_blackboard_decorator`, `behavior_tree.add_service`, `behavior_tree.connect`, `behavior_tree.move_child`, `behavior_tree.set_node_property`, `behavior_tree.remove_subtree`, and `behavior_tree.layout`, then save the Behavior Tree and Blackboard assets.
- Prefer `behavior_tree.add_blackboard_decorator` for Blackboard key comparisons. It accepts `key`, `operation`, `value`, `abort_mode`, and `index`; use it instead of guessing `BTDecorator_Blackboard` reflected property shapes. Use `insert_index` when adding task/composite/wait children, `behavior_tree.move_child` when preserving execution order, and `behavior_tree.remove_subtree` or `remove_node` with `remove_children=true` when deleting a branch.
- `behavior_tree.set_node_property` returns `resolved_target` metadata. If a property does not exist on the resolved object, treat the call as failed and use the returned property suggestions instead of assuming the write succeeded.
- For Blackboard object/class keys, prefer `blackboard.add_key` with `type` set to `object` or `class`, then set key-type reflected fields such as `BaseClass` through `properties` or `blackboard.set_key_property`. Use concrete project task/service/decorator class paths when authoring project-specific AI trees.
- For StateTree authoring in UE 5.8, call `statetree.create` without `schema_class_path` for the default component schema, then use `statetree.add_state`, `statetree.add_task`, `statetree.add_evaluator`, `statetree.add_transition`, `statetree.set_node_property`, `statetree.compile`, and `statetree.inspect` for verification. Task/evaluator calls need public `UScriptStruct` paths. UE 5.7 builds intentionally omit `statetree*` operations from the catalog.

## Input, Material, And Niagara

- For Enhanced Input mappings, call `input.mapping.list` after edits and use `input.mapping.set_trigger_property` or `input.mapping.set_modifier_property` for reflected trigger/modifier object properties.
- For Material graph edits, prefer `material.graph.add_parameter`, `material.graph.set_texture`, and `material.graph.add_function_call` for common parameter/texture/function work. Verify `material.graph.connect` through `material.graph.list` connection metadata before `material.graph.compile` and `asset.save`.
- For Niagara stack work, inspect first with `niagara.stack.inspect`; use node GUIDs or indexes for `niagara.stack.move_module`, `niagara.stack.set_module_enabled`, and `niagara.stack.remove_module`. Prefer `niagara.stack.add_parameter_module` for agent-authored Set Parameters modules. Use `niagara.stack.add_module` only when you have an existing Niagara Script module asset path. Use `niagara.scratchpad.inspect`, `niagara.scratchpad.add_script`, and `niagara.scratchpad.remove_script` for scratch pad script list edits, then save the Niagara System.

## Property Setters, Tasks, And Python

- Existing-asset operations, including `behavior_tree.inspect` and `blackboard.inspect`, accept both `asset_path` and `asset`.
- Property setters such as `behavior_tree.set_node_property`, `blackboard.set_key_property`, `asset.set_property`, Blueprint/Material/Animation graph node property setters, Widget property setters, Input trigger/modifier setters, SoundCue node property setters, and StateTree property setters return before/after property export text when possible; use that response when visible editor text looks stale.
- Use `task.submit` when you need queued editor-thread execution with stored result lookup. It is useful for polling `queued`, `running`, `completed`, `failed`, or `canceled` state, but it is not a worker-thread path for unsafe parallel mutation.
- Use `python.execute_script` for multiline Python. Provide exactly one of `code`, `code_base64`, or `script_path`; `script_path` must point to a `.py` file under the project directory or project `Saved/` directory. Set `capture_output=true` when stdout/stderr are needed in the MCP response.
- Check `unreal_status`, `unreal_context`, or `python.status` before Python work. If `python.enabled=false`, relaunch Unreal Editor with `Scripts/Start-UnreelMCPProject.ps1 -EnablePython`; bridge-side environment variables alone do not change an already running editor process.
- Use `script.history` to inspect recent `python.execute` and `python.execute_script` attempts.
