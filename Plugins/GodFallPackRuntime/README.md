# GodFall pack runtime

Native UE5.8 Win64 module for GodFall_Clean. It is disabled by default; the base game must explicitly enable and compile it, cook the base schemas, and include the Blueprint integration. This source is under integration and is **not yet verified with a packaged game or real cooked pack**.

## Pack entry point

For each `Example.pak`, publish an adjacent `Example.pack.json`:

```json
{"schema_version":1,"catalog":"/Game/GFExternal/Example/DA_Example.DA_Example"}
```

The catalog is a cooked `GodFallPackDefinition` DataAsset. Its `PackId` must be unique. Put all new definitions under `/Game/GFExternal/`; never include replacement base-game catalogs or packages.

- `CharacterProfiles`: instances of the existing `BP_GF_GodCharacterProfile`, with unique `GFEXT_` GodIds and compatible authored presentation/body, animation and stats. These represent new characters. Starter clothing and character-specific definitions must use the new GodId's existing compatibility policy; changing a copied profile's ID does not retarget its clothing.
- `ClothingDefinitions`: instances of `BP_GF_ArmorDefinition`, with unique `GFEXT_` DefinitionIds and existing category, character, mesh, coverage and loot-eligibility metadata. Existing matching and loot rules remain authoritative.
- `WeaponDefinitions`: classes derived from `BP_GF_WeaponDefinition`, with unique `GFEXT_` DefinitionIds and WeaponClassIds, a compatible combat profile, mesh, socket and grip transform. Existing weapon-instance creation owns rolls and identity.
- `ItemTables`: optional authored acquisition-table extensions. Each binding names an existing `TargetTable` under `/Game/SYN/Content/InventorySystem/DataTables/` and an `AdditionalRows` DataTable under `/Game/GFExternal/` with the exact same row struct. Every new row name must start with `GFEXT_` and be unique across packs. Author the existing S_Item prices, recipes, ingredients and equipment fields; definitions alone do not invent recipes or prices. No items are granted on startup.

For a weapon acquisition row, use the weapon's `DefinitionId` as its row name, existing weapon category/equipment metadata, and `BaseInfo.MaxStackSize=1`. Weapon DefinitionIds must be unique. The inventory bridge calls the existing permanent weapon-instance factory once per stored slot, retaining the complete authored item template and generating its identity/rolls there. Table lookup alone does not create a rolled weapon. Invalid stack sizes leave the base table in use. Private PIE checks passed actual stored payload, unique identity, capacity rejection, equipment and fresh Continue without rerolling.

Runtime queries use transient catalog/table copies. Base rows cannot be shadowed. Composite item-table queries include extensions targeting their constituent tables. Generic weapon pickups may include external definitions; specialized pickup subclasses keep their configured pools.

## Base game integration status

The plugin is a support module; the base GodFall build needs matching Blueprint routes and a rebuild. In the live project, the GameInstance startup/profile/Continue routes, armor catalog's combined-definition queries, and conditional character-entry wiring are saved, compiled and mapped. Editor fixtures passed additional-character selection, gameplay profile resolution, fresh Continue, unavailable-character rejection, clothing eligibility, Back/focus, and adjacent base preview checks. Fixtures reuse existing art and do not prove cooked-pack mounting.

Item-table routes in the shared lookup, inventory, transaction manager, crafting list and shop list are saved and compiled. Private PIE checks passed external permanent weapon acquisition, generic pickup collection, specialized Dagger protection, insufficient-ingredient rejection and exact recipe costs, paid base-shop affordability/deduction, full-bag rejection, equipment and fresh-save restoration. Imported-character clothing passed existing loot selection, collection, category equipment and visible-mesh checks. Author-defined zero prices remain zero; no prices, recipes or startup inventory grants are invented.

These checks use disposable catalog fixtures and existing art. Actual cooked-pack admission/removal in a rebuilt packaged Windows game is the next verification step. The runtime logs registry counts and its save directory on first initialization so packaged startup can be checked directly.

`InstallExternalCharacterEntry` is intended for the existing selection widget's Event Construct. It adds one runtime button to `GF_RealmDesign` only when external characters exist and owns its click delegate. The designer hierarchy and existing carousel bindings remain authored by the base game.

The registry reads the launcher's owned `.godfall-github.json` after startup pack discovery. Invalid or conflicting catalogs are rejected as a unit. A fresh process builds its registry only from the installed set. Missing imported character IDs fail the continuation availability gate. Save files are retained; full equipped-item removal behavior still requires verification.

## Packaging

Build cooked classic Paks against the exact base GodFall release, engine build and Win64 target, with IoStore disabled. Include the catalog and all its soft-referenced external assets in the cook. Do not bundle copies of base content. Keep packs at most95MiB for the current GitHub publisher. Upload the `.pak` and its descriptor together to `GameItems/Models`, `Clothing` or `Weapons`, then wait for the publishing workflow.

The category folder organizes uploads; the catalog declares the actual content. Raw meshes, editor-only assets and uncooked packages cannot substitute for cooked game definitions. A packaged add/remove test remains required before release.
