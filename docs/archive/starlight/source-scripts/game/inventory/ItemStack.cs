namespace Starlight.Game.Inventory;

public sealed class ItemStack
{
    public string ItemId { get; set; } = "";
    public float Count { get; set; } = 0f;

    public ItemStack() { }

    public ItemStack(string itemId, float count)
    {
        ItemId = itemId;
        Count = count;
    }

    public bool IsEmpty => string.IsNullOrEmpty(ItemId) || Count <= 0f;
}
