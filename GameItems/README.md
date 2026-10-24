# Game Items

- `Models/`: prepared character and other model packs.
- `Clothing/`: prepared wearable packs.
- `Weapons/`: prepared weapon packs.

The `.gitkeep` files keep the empty directories visible in GitHub. They are placeholders, not game items.

## Intended startup behavior

Upload compatible cooked `.pak` files to these folders. The **Publish GodFall item list** workflow updates `manifest.json` after additions, changes, and removals. The [startup launcher](../launcher/README.md) synchronizes that published list before starting a packaged game and retains verified cached packs on a failed connection.

Playable-item registration is still pending. Follow the launcher's compatibility and packaging requirements before publishing real content; source models and a mesh-only `.pak` do not define a complete playable item.

Existing project assets have not been moved or deleted. The launcher removes only its own downloaded packs after a successfully published update; it does not delete source project assets or saves.
