# Unreal MCP Setup

The project uses `ChiR24/Unreal_mcp` through the local `McpAutomationBridge` plugin.

## Current Setup

- Plugin path: `Plugins/McpAutomationBridge`
- Enabled plugins in `Stargame.uproject`:
  - `EnhancedInput`
  - `ModelingToolsEditorMode`
  - `PythonScriptPlugin`
  - `McpAutomationBridge`
  - `EditorScriptingUtilities`
  - `Niagara`
- Native MCP is enabled in `Config/DefaultGame.ini`
- MCP client config lives in `.mcp.json`
- Native MCP endpoint:

```text
http://localhost:3000/mcp
```

## Starting The Bridge

1. Open `Stargame.uproject` in Unreal Editor.
2. Wait for the editor to finish loading the project.
3. Check the editor status bar for MCP on port `3000`.
4. Connect MCP clients to:

```text
http://localhost:3000/mcp
```

The bridge is loopback-only:

- `ListenHost=127.0.0.1`
- `bAllowNonLoopback=False`

Keep it that way unless remote editor automation is deliberately required.

## Expected Use

Use MCP for editor-side verification and content operations:

- inspect assets
- inspect levels
- place or query actors
- run PIE/editor checks
- capture viewport screenshots
- validate DataAssets and generated fixture content
- inspect Blueprint/Material/Niagara setup

Do not use MCP as a replacement for runtime architecture. Canonical system state, generation, transitions, scale conversion, docking, supercruise, and save/load still belong in C++/DataAssets as defined by the architecture and build docs.

MCP-created or MCP-modified assets must pass the same C++ validation/editor utility path as manually authored assets and must be source-controlled. MCP should not become the only way to produce fixture content.

See `content-validation-and-tooling.md` for the required validation workflow for MCP-created assets.

## Build Verification

Known verified setup:

- Unreal Engine 5.7.4
- `StargameEditor` builds successfully
- plugin links as `libUnrealEditor-McpAutomationBridge.so`
- upstream warning: plugin depends on UE's deprecated `StructUtils` plugin, but it still builds on 5.7

## Client Config

`.mcp.json` should contain:

```json
{
  "mcpServers": {
    "unreal-engine": {
      "type": "url",
      "url": "http://localhost:3000/mcp"
    }
  }
}
```

## Build Workflow Role

MCP is useful for editor-side verification once the fixture system exists:

- verify `frontier_test_01` loads in editor/PIE
- inspect spawned registry actors and DataAsset references
- capture map/debug overlays and validate coordinate/frame visualization
- verify target selection and local approach actors
- inspect supercruise debug telemetry in PIE
- verify docking port transforms and docked/undocked state

If MCP actions create or modify assets, those changes must still follow the C++/Blueprint ownership rules in `cpp-blueprint-ownership.md`, the data rules in `system-data-contracts.md`, and the validation workflow in `content-validation-and-tooling.md`.
