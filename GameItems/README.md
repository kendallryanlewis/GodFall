# Historical item-pack exports

These cooked packs belong to the retired GitHub-download workflow. They are preserved as historical files; adding, changing or deleting them no longer affects the current editor/game item catalog. Automatic manifest publication is removed.

Manage editable items in Unreal instead:

- Armor: `/Game/GF/Items/Armor/Data/DA_GF_ArmorCatalog` and its definition assets.
- Weapons: `/Game/GF/Weapons/Definitions`; loot membership is authored on generic or specialized weapon pickups.
- Free store: the local Sword/Dagger definitions and armor catalog. Additional weapon membership currently lives in `GetFreeStoreDefinitions`.

GitHub stores versioned source changes. Do not upload raw models or rebuild these packs to update editor gameplay. Import and configure assets in Unreal, save, and use source control.
