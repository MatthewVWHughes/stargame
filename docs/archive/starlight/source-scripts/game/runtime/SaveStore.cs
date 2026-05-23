using Godot;

namespace Starlight.Game.Runtime;

/// <summary>
/// Abstraction over the save-file byte store. Production uses
/// <see cref="GodotFileSaveStore"/> against Godot's user:// filesystem;
/// tests use <see cref="InMemorySaveStore"/> so they don't need the
/// Godot runtime. Swap via <see cref="SaveService.Store"/>.
/// </summary>
public interface ISaveStore
{
    bool Exists();
    string Read();
    void Write(string json);
    void Delete();
}

/// <summary>
/// Default store: writes to Godot's user:// virtual filesystem.
/// </summary>
public sealed class GodotFileSaveStore : ISaveStore
{
    private const string Path = "user://vs1_save.json";

    public bool Exists() => FileAccess.FileExists(Path);

    public string Read()
    {
        using FileAccess file = FileAccess.Open(Path, FileAccess.ModeFlags.Read);
        return file.GetAsText();
    }

    public void Write(string json)
    {
        using FileAccess file = FileAccess.Open(Path, FileAccess.ModeFlags.Write);
        file.StoreString(json);
    }

    public void Delete()
    {
        if (!FileAccess.FileExists(Path))
            return;
        DirAccess.RemoveAbsolute(ProjectSettings.GlobalizePath(Path));
    }
}

/// <summary>
/// Test-only store: keeps the payload in process memory. Not
/// persistent across runs. Safe to use in unit tests without
/// needing the Godot runtime.
/// </summary>
public sealed class InMemorySaveStore : ISaveStore
{
    private string _json;

    public bool Exists() => _json != null;

    public string Read() => _json;

    public void Write(string json) => _json = json;

    public void Delete() => _json = null;
}
