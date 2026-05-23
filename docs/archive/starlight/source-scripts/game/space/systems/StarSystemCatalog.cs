using System.Collections.Generic;

namespace Starlight.Game.Space;

// Lookup table mapping system ids ("sol", "alpha_centauri", ...) to JSON
// data files, with lazy-loaded caching.
public static class StarSystemCatalog
{
	private static readonly Dictionary<string, string> _idToDataPath = new()
	{
		{ "sol", "res://data/systems/sol.json" },
		{ "alpha_centauri", "res://data/systems/alpha_centauri.json" },
	};

	private static readonly Dictionary<string, StarSystemDefinition> _cache = new();

	public static StarSystemDefinition Get(string id)
	{
		if (_cache.TryGetValue(id, out StarSystemDefinition cached))
			return cached;

		string path = _idToDataPath.TryGetValue(id, out string mapped)
			? mapped
			: _idToDataPath["sol"];

		StarSystemDefinition definition = StarSystemLoader.LoadFromJson(path);
		_cache[id] = definition;
		return definition;
	}

	public static IEnumerable<string> KnownIds => _idToDataPath.Keys;
}
