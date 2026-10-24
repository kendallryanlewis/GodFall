# Game Items

- `Models/`: character and other model content.
- `Clothing/`: wearable content.
- `Weapons/`: weapon content.

The `.gitkeep` files keep the empty directories visible in GitHub. They are placeholders, not game items.

## Intended startup behavior

Once the game integration is implemented, GodFall should download new or changed supported items, make them available in the appropriate game lists, and remove unavailable items from those lists after a successful synchronization. A failed connection should retain the last successfully validated local content.

Existing project assets have not been moved or deleted. Removing a file here does not currently change the game or delete project assets.
