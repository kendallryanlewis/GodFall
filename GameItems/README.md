# Game Items

- `Models/`: prepared character and other model packs.
- `Armor/`: prepared armor packs.
- `Weapons/`: prepared weapon packs.

`Armor/LocalArmor.pak` publishes the current 54 armor definitions. `Weapons/LocalWeapons.pak` publishes the current Sword and Dagger definitions. Their catalogs live under `/Game/GFExternal/LocalEquipment/` and retain the local items' appearance, stats, loot eligibility and character restrictions. Weapon labels retain their original names.

These definition packs require the compatible GodFall_Clean UE 5.8 base game, which supplies their meshes, materials, animations and parent Blueprint policies. Use the [equipment catalog release](https://github.com/kendallryanlewis/GodFall/releases/tag/godfall-content-20261025-equipment-catalog) or the matching Clean editor project. It includes the catalog-query fix needed for Continue with the full armor catalog. The packs do not contain standalone raw models. Original local definitions remain available for fallback and existing owned items.

The `.gitkeep` files are directory placeholders, not game items.

## Intended startup behavior

Upload compatible cooked `.pak` files and adjacent same-basename `.pack.json` descriptors to these folders. Each descriptor names its cooked catalog, for example `{"schema_version":1,"catalog":"/Game/GFExternal/Hero/DA_Hero.DA_Hero"}`. The **Publish GodFall item list** workflow updates `manifest.json` after additions, changes, and removals. The [startup launcher](../launcher/README.md) synchronizes that published list before starting a packaged game and retains verified cached packs on a failed connection.

Follow the [runtime catalog contract](../Plugins/GodFallPackRuntime/README.md) and launcher's compatibility requirements before publishing additional content; source models and a mesh-only `.pak` do not define a complete playable item. The item offer categories are armor and weapons. GitHub items supply generic loot and the Quartermaster's free store when available; an empty or unavailable catalog selects local items. Existing eligibility, character compatibility, paid-shop and crafting rules remain authoritative. Disposable verification packs are not published here.

`Clothing/` and the catalog field `ClothingDefinitions` remain accepted legacy names for armor compatibility. They do not add a separate clothing offer category.

Existing project assets have not been moved or deleted. The launcher removes only its own downloaded packs after a successfully published update; it does not delete source project assets or saves.
