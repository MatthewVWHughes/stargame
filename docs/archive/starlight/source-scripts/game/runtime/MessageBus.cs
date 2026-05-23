using Godot;
using MessagePipe;
using Microsoft.Extensions.DependencyInjection;
using System;

namespace Starlight.Game.Runtime;

/// <summary>
/// Autoload that bootstraps MessagePipe's GlobalMessagePipe so any C# code
/// can Publish / Subscribe without passing a container around.
///
/// Register as an autoload in project.godot. The Publish / Subscribe helpers
/// below are thin wrappers over GlobalMessagePipe; either API is fine.
/// </summary>
public partial class MessageBus : Node
{
    private static bool s_initialized;
    private static IServiceProvider _provider;

    public override void _EnterTree()
    {
        if (s_initialized)
            return;

        var services = new ServiceCollection();
        services.AddMessagePipe();

        _provider = services.BuildServiceProvider();
        GlobalMessagePipe.SetProvider(_provider);

        s_initialized = true;
        GD.Print("MessageBus: GlobalMessagePipe ready.");
    }

    public override void _ExitTree()
    {
        (_provider as IDisposable)?.Dispose();
        _provider = null;
        s_initialized = false;
    }

    /// <summary>Publish an event to all subscribers.</summary>
    public static void Publish<T>(T message)
    {
        if (!s_initialized)
        {
            // stderr fallback — GD.PushWarning requires the Godot runtime,
            // which isn't initialized under VSTest-based unit tests.
            System.Console.Error.WriteLine(
                $"MessageBus.Publish<{typeof(T).Name}> called before autoload init — dropping.");
            return;
        }
        GlobalMessagePipe.GetPublisher<T>().Publish(message);
    }

    /// <summary>Subscribe to an event type. Dispose the returned handle to unsubscribe.</summary>
    public static IDisposable Subscribe<T>(Action<T> handler)
    {
        if (!s_initialized)
        {
            System.Console.Error.WriteLine(
                $"MessageBus.Subscribe<{typeof(T).Name}> called before autoload init — no-op.");
            return EmptyDisposable.Instance;
        }
        return GlobalMessagePipe.GetSubscriber<T>().Subscribe(handler);
    }

    private sealed class EmptyDisposable : IDisposable
    {
        public static readonly EmptyDisposable Instance = new();
        public void Dispose() { }
    }
}
