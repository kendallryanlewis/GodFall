# GodFall pack runtime

Native UE5.8 Win64 module for GodFall_Clean. It is disabled by default; the base game must explicitly enable and compile it, cook the base schemas, and include the Blueprint integration. This source is under integration and is **not yet verified with a packaged game or real cooked pack**.

## Pack entry point

For each `Example.pak`, publish an adjacent `Example.pack.json`:

```json
{"schema_version":1,"catalog":"/Game/GFExternal/Example/DA_Example.DA_Example"}
```

The catalog is a cooked `GodFallPackDefinition` DataAsset. Its `PackId` must be unique. Put all new definitions under `/Game/GFExternal/`; never include replacement base-game catalogs or packages.

- `CharacterProfiles`: instances of the existing `BP_GF_GodCharacterProfile`, with unique `GFEXT_` GodIds and compatible authored presentation/body, animation and stats. These represent new characters.
- `ClothingDefinitions`: instances of `BP_GF_ArmorDefinition`, with unique `GFEXT_` DefinitionIds and existing category, character, mesh, coverage and loot-eligibility metadata. Existing matching and loot rules remain authoritative.
- `WeaponDefinitions`: classes derived from `BP_GF_WeaponDefinition`, with unique `GFEXT_` DefinitionIds and WeaponClassIds, a compatible combat profile, mesh, socket and grip transform. Existing weapon-instance creation owns rolls and identity.
- `ItemTables`: optional authored acquisition-table extensions. Each binding names an existing `TargetTable` under `/Game/SYN/Content/InventorySystem/DataTables/` and an `AdditionalRows` DataTable under `/Game/GFExternal/` with the exact same row struct. Every new row name must start with `GFEXT_` and be unique across packs. Author the existing S_Item prices, recipes, ingredients and equipment fields; definitions alone do not invent recipes or prices. No items are granted on startup.

Runtime queries use transient catalog/table copies. Base rows cannot be shadowed. Composite item-table queries include extensions targeting their constituent tables. Generic weapon pickups may include external definitions; specialized pickup subclasses keep their configured pools.

The registry reads the launcher's owned `.godfall-github.json` after startup pack discovery. Invalid or conflicting catalogs are rejected as a unit. A fresh process builds its registry only from the installed set. Missing imported character IDs fail the continuation availability gate. Save files are retained; full equipped-item removal behavior still requires verification.

## Packaging

Build cooked classic Paks against the exact base GodFall release, engine build and Win64 target, with IoStore disabled. Include the catalog and all its soft-referenced external assets in the cook. Do not bundle copies of base content. Keep packs at most95MiB for the current GitHub publisher. Upload the `.pak` and its descriptor together to `GameItems/Models`, `Clothing` or `Weapons`, then wait for the publishing workflow.

The category folder organizes uploads; the catalog declares the actual content. Raw meshes, editor-only assets and uncooked packages cannot substitute for cooked game definitions. A packaged add/remove test remains required before release.
