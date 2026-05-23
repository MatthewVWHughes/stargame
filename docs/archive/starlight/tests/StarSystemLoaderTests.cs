using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Space;

namespace Starlight.Tests;

/// <summary>
/// Tests the JSON deserialization path and custom converters (Color, Vector3,
/// nullable variants) used by <see cref="StarSystemLoader"/>.
/// Uses <see cref="StarSystemLoader.LoadFromJsonString"/> to bypass Godot's
/// FileAccess, which requires the runtime.
/// </summary>
[TestSuite]
public class StarSystemLoaderTests
{
    [TestCase]
    public void LoadFromJsonString_MinimalDefinition_ParsesIdAndDisplayName()
    {
        string json = "{\"id\":\"sol\",\"display_name\":\"Sol\",\"primary_star_gravity_radius\":2500}";

        StarSystemDefinition def = StarSystemLoader.LoadFromJsonString(json);

        AssertThat(def).IsNotNull();
        AssertThat(def.Id).IsEqual("sol");
        AssertThat(def.DisplayName).IsEqual("Sol");
        AssertThat(def.PrimaryStarGravityRadius).IsEqual(2500f);
    }

    [TestCase]
    public void LoadFromJsonString_BodyWithOrbit_ParsesOrbit()
    {
        string json = @"{
            ""id"":""sol"",
            ""bodies"":[{
                ""name"":""earth"",
                ""display_name"":""Earth"",
                ""orbit"":{
                    ""semi_major_axis"":150,
                    ""period"":365,
                    ""phase_offset"":0.5,
                    ""eccentricity"":0.02,
                    ""inclination"":1.5
                }
            }]
        }";

        StarSystemDefinition def = StarSystemLoader.LoadFromJsonString(json);

        AssertThat(def.Bodies.Count).IsEqual(1);
        var earth = def.Bodies[0];
        AssertThat(earth.Name).IsEqual("earth");
        AssertThat(earth.DisplayName).IsEqual("Earth");
        AssertThat(earth.Orbit).IsNotNull();
        AssertThat(earth.Orbit.SemiMajorAxis).IsEqual(150f);
        AssertThat(earth.Orbit.Period).IsEqual(365f);
        AssertThat(earth.Orbit.Eccentricity).IsEqual(0.02f);
    }

    [TestCase]
    public void LoadFromJsonString_Vector3AsArray_Parses()
    {
        string json = @"{
            ""id"":""sol"",
            ""bodies"":[{
                ""name"":""sun"",
                ""scene_visual"":{
                    ""node_name"":""sun"",
                    ""scene_path"":""res://sun.tscn"",
                    ""scale"":[2.0, 2.0, 2.0]
                }
            }]
        }";

        StarSystemDefinition def = StarSystemLoader.LoadFromJsonString(json);

        var visual = def.Bodies[0].SceneVisual;
        AssertThat(visual).IsNotNull();
        AssertThat(visual.Scale).IsNotNull();
        var scale = visual.Scale!.Value;
        AssertThat(scale.X).IsEqual(2.0f);
        AssertThat(scale.Y).IsEqual(2.0f);
        AssertThat(scale.Z).IsEqual(2.0f);
    }

    [TestCase]
    public void LoadFromJsonString_Vector3AsSingleScalarArray_ExpandsUniform()
    {
        string json = @"{
            ""id"":""sol"",
            ""bodies"":[{
                ""name"":""sun"",
                ""scene_visual"":{
                    ""node_name"":""sun"",
                    ""scene_path"":""res://sun.tscn"",
                    ""scale"":[3.0]
                }
            }]
        }";

        StarSystemDefinition def = StarSystemLoader.LoadFromJsonString(json);

        var visual = def.Bodies[0].SceneVisual;
        var scale = visual.Scale!.Value;
        AssertThat(scale.X).IsEqual(3.0f);
        AssertThat(scale.Y).IsEqual(3.0f);
        AssertThat(scale.Z).IsEqual(3.0f);
    }

    [TestCase]
    public void LoadFromJsonString_NumericFromString_Reads()
    {
        string json = "{\"id\":\"sol\",\"primary_star_gravity_radius\":\"1800\"}";

        StarSystemDefinition def = StarSystemLoader.LoadFromJsonString(json);

        AssertThat(def.PrimaryStarGravityRadius).IsEqual(1800f);
    }

    [TestCase]
    public void LoadFromJsonString_JumpGates_Parses()
    {
        string json = @"{
            ""id"":""sol"",
            ""jump_gates"":[{
                ""gate_id"":""gate_alpha"",
                ""destination_system_id"":""alpha_centauri"",
                ""arrival_gate_id"":""gate_sol"",
                ""display_name"":""Alpha Gate"",
                ""anchor_body"":""earth"",
                ""distance"":1000,
                ""activation_range"":500
            }]
        }";

        StarSystemDefinition def = StarSystemLoader.LoadFromJsonString(json);

        AssertThat(def.JumpGates.Count).IsEqual(1);
        var gate = def.JumpGates[0];
        AssertThat(gate.GateId).IsEqual("gate_alpha");
        AssertThat(gate.DestinationSystemId).IsEqual("alpha_centauri");
        AssertThat(gate.Distance).IsEqual(1000f);
        AssertThat(gate.ActivationRange).IsEqual(500f);
    }

    [TestCase]
    public void LoadFromJsonString_CommentsAndTrailingCommas_Allowed()
    {
        string json = @"{
            /* comment */
            ""id"":""sol"",
            ""display_name"":""Sol"",
        }";

        StarSystemDefinition def = StarSystemLoader.LoadFromJsonString(json);

        AssertThat(def.Id).IsEqual("sol");
    }
}
