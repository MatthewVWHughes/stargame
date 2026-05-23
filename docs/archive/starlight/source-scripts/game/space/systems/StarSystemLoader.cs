using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using Godot;

namespace Starlight.Game.Space;

// Deserialises a StarSystemDefinition from a JSON file on the Godot
// virtual filesystem (res://...). See docs/systems/star_system_schema.md.
public static class StarSystemLoader
{
	private static readonly JsonSerializerOptions _options = BuildOptions();

	public static StarSystemDefinition LoadFromJson(string resourcePath)
	{
		using FileAccess file = FileAccess.Open(resourcePath, FileAccess.ModeFlags.Read);
		if (file == null)
			throw new InvalidOperationException(
				$"StarSystemLoader: could not open {resourcePath} ({FileAccess.GetOpenError()})");

		return LoadFromJsonString(file.GetAsText(), resourcePath);
	}

	/// <summary>
	/// Deserializes a <see cref="StarSystemDefinition"/> from a JSON string.
	/// Split from <see cref="LoadFromJson"/> so tests can exercise the
	/// converters without touching Godot's virtual filesystem.
	/// </summary>
	public static StarSystemDefinition LoadFromJsonString(string json, string originLabel = "<in-memory>")
	{
		StarSystemDefinition definition = JsonSerializer.Deserialize<StarSystemDefinition>(json, _options);
		if (definition == null)
			throw new InvalidOperationException($"StarSystemLoader: {originLabel} parsed to null");

		return definition;
	}

	private static JsonSerializerOptions BuildOptions()
	{
		var options = new JsonSerializerOptions
		{
			PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
			ReadCommentHandling = JsonCommentHandling.Skip,
			AllowTrailingCommas = true,
			NumberHandling = JsonNumberHandling.AllowReadingFromString,
		};
		options.Converters.Add(new ColorJsonConverter());
		options.Converters.Add(new Vector3JsonConverter());
		options.Converters.Add(new NullableVector3JsonConverter());
		options.Converters.Add(new NullableColorJsonConverter());
		return options;
	}

	private sealed class ColorJsonConverter : JsonConverter<Color>
	{
		public override Color Read(ref Utf8JsonReader reader, Type type, JsonSerializerOptions options)
			=> ParseColor(ref reader);

		public override void Write(Utf8JsonWriter writer, Color value, JsonSerializerOptions options)
			=> writer.WriteStringValue($"#{value.ToHtml(includeAlpha: value.A < 1f)}");
	}

	private sealed class NullableColorJsonConverter : JsonConverter<Color?>
	{
		public override Color? Read(ref Utf8JsonReader reader, Type type, JsonSerializerOptions options)
		{
			if (reader.TokenType == JsonTokenType.Null)
				return null;
			return ParseColor(ref reader);
		}

		public override void Write(Utf8JsonWriter writer, Color? value, JsonSerializerOptions options)
		{
			if (value == null)
				writer.WriteNullValue();
			else
				writer.WriteStringValue($"#{value.Value.ToHtml(includeAlpha: value.Value.A < 1f)}");
		}
	}

	private static Color ParseColor(ref Utf8JsonReader reader)
	{
		// Hex string ("#rrggbb" / "#rrggbbaa") or object ({"r":.., "g":.., "b":.., "a":..}).
		if (reader.TokenType == JsonTokenType.String)
		{
			string s = reader.GetString() ?? "";
			if (s.StartsWith('#'))
				s = s[1..];
			return Color.FromHtml(s);
		}

		if (reader.TokenType == JsonTokenType.StartObject)
		{
			float r = 0f, g = 0f, b = 0f, a = 1f;
			while (reader.Read() && reader.TokenType != JsonTokenType.EndObject)
			{
				if (reader.TokenType != JsonTokenType.PropertyName) continue;
				string key = reader.GetString() ?? "";
				reader.Read();
				float v = reader.GetSingle();
				switch (key)
				{
					case "r": r = v; break;
					case "g": g = v; break;
					case "b": b = v; break;
					case "a": a = v; break;
				}
			}
			return new Color(r, g, b, a);
		}

		throw new JsonException($"Unexpected token for Color: {reader.TokenType}");
	}

	private sealed class Vector3JsonConverter : JsonConverter<Vector3>
	{
		public override Vector3 Read(ref Utf8JsonReader reader, Type type, JsonSerializerOptions options)
			=> ParseVector3(ref reader);

		public override void Write(Utf8JsonWriter writer, Vector3 value, JsonSerializerOptions options)
		{
			writer.WriteStartArray();
			writer.WriteNumberValue(value.X);
			writer.WriteNumberValue(value.Y);
			writer.WriteNumberValue(value.Z);
			writer.WriteEndArray();
		}
	}

	private sealed class NullableVector3JsonConverter : JsonConverter<Vector3?>
	{
		public override Vector3? Read(ref Utf8JsonReader reader, Type type, JsonSerializerOptions options)
		{
			if (reader.TokenType == JsonTokenType.Null)
				return null;
			return ParseVector3(ref reader);
		}

		public override void Write(Utf8JsonWriter writer, Vector3? value, JsonSerializerOptions options)
		{
			if (value == null)
			{
				writer.WriteNullValue();
				return;
			}

			Vector3 v = value.Value;
			writer.WriteStartArray();
			writer.WriteNumberValue(v.X);
			writer.WriteNumberValue(v.Y);
			writer.WriteNumberValue(v.Z);
			writer.WriteEndArray();
		}
	}

	private static Vector3 ParseVector3(ref Utf8JsonReader reader)
	{
		if (reader.TokenType == JsonTokenType.StartArray)
		{
			var components = new List<float>(3);
			while (reader.Read() && reader.TokenType != JsonTokenType.EndArray)
			{
				if (reader.TokenType == JsonTokenType.Number)
					components.Add(reader.GetSingle());
				else if (reader.TokenType == JsonTokenType.String
					&& float.TryParse(reader.GetString(), NumberStyles.Float, CultureInfo.InvariantCulture, out float parsed))
					components.Add(parsed);
			}

			if (components.Count == 3)
				return new Vector3(components[0], components[1], components[2]);
			if (components.Count == 1)
				return new Vector3(components[0], components[0], components[0]);

			throw new JsonException($"Vector3 array must have 1 or 3 components, got {components.Count}");
		}

		if (reader.TokenType == JsonTokenType.StartObject)
		{
			float x = 0f, y = 0f, z = 0f;
			while (reader.Read() && reader.TokenType != JsonTokenType.EndObject)
			{
				if (reader.TokenType != JsonTokenType.PropertyName) continue;
				string key = reader.GetString() ?? "";
				reader.Read();
				float v = reader.GetSingle();
				switch (key)
				{
					case "x": x = v; break;
					case "y": y = v; break;
					case "z": z = v; break;
				}
			}
			return new Vector3(x, y, z);
		}

		throw new JsonException($"Unexpected token for Vector3: {reader.TokenType}");
	}
}
