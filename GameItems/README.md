# Game Items

- `Models/`: prepared character and other model packs.
- `Armor/`: prepared armor packs.
- `Weapons/`: prepared weapon packs.

The `.gitkeep` files keep the empty directories visible in GitHub. They are placeholders, not game items.

## Intended startup behavior

Upload compatible cooked `.pak` files and adjacent same-basename `.pack.json` descriptors to these folders. Each descriptor names its cooked catalog, for example `{"schema_version":1,"catalog":"/Game/GFExternal/Hero/DA_Hero.DA_Hero"}`. The **Publish GodFall item list** workflow updates `manifest.json` after additions, changes, and removals. The [startup launcher](../launcher/README.md) synchronizes that published list before starting a packaged game and retains verified cached packs on a failed connection.

Follow the [runtime catalog contract](../Plugins/GodFallPackRuntime/README.md) and launcher's compatibility requirements before publishing real content; source models and a mesh-only `.pak` do not define a complete playable item. The item offer categories are armor and weapons. GitHub items supply generic loot and the Quartermaster's free store when available; an empty or unavailable catalog selects local items. Existing eligibility, character compatibility, paid-shop and crafting rules remain authoritative. These folders are ready for your packs; disposable verification packs are not published here.

`Clothing/` and the catalog field `ClothingDefinitions` remain accepted legacy names for armor compatibility. They do not add a separate clothing offer category.

Existing project assets have not been moved or deleted. The launcher removes only its own downloaded packs after a successfully published update; it does not delete source project assets or saves.
