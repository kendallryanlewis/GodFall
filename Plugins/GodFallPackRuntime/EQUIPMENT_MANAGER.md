# Equipment Manager

In GodFall_Clean, open **Tools → GodFall Equipment Manager**.

The panel contains the equipment list, a mesh preview, definition properties, and availability controls. Stop Play before making changes.

1. **Import armor…** uses the existing character/slot importer. Skinned armor must already be fitted and weighted to the configured character skeleton. Rigid armor attachments use the existing fit-and-approve workflow; select the draft mesh in the Content Browser and use **Fit selected armor draft**, then **Approve fit**.
2. **Import weapon…** opens Unreal's import dialog and registers each imported Static Mesh as a weapon draft. It starts from the existing Sword combat-style template. Review its combat profile, stats, socket and grip transform before enabling it. Importing geometry does not generate animations or a new combat style.
3. **Add selected asset** registers an existing imported mesh, or selects an existing registered definition. Armor meshes must use the existing character/region folders. Weapon definitions live under `/Game/GF/Weapons/Definitions`.
4. Select an item to preview its mesh and edit its definition. **Save changes** compiles weapon definitions, saves the asset, and synchronizes its Blueprint map. Stable definition and weapon-class IDs are read-only to protect saved items.
5. Choose **Available**, **Loot**, and **Free store** independently. These availability changes save immediately. Armor loot still respects its character compatibility and **Allow Random Loot** property. **Random weapon pool** controls whether a loot-enabled weapon can appear in generic random pickups; specialized pickups retain their authored restrictions.
6. **Remove / restore** toggles future acquisition. It retains the definition, source art and metadata used by existing inventory items. It does not delete files. New imports are drafts until enabled.

Press **Play** to use the saved settings. The Quartermaster reads the managed free-store list; armor and weapon pickups read the loot settings. Claims retain the existing zero-cost purchase, item-roll and save behavior.

The acquisition settings are saved in `/Game/GF/Items/DA_GF_EquipmentManager`. Armor definitions retain their existing catalog and weapon definitions retain their existing Blueprint schema. GitHub is version control only; this panel does not download assets or build executables. Commit authored `.uasset` files and source changes through the project's version-control workflow.

The editor actions reuse GodFall_Clean's installed `GodFallWearables` plugin and the workspace's Blueprint map utilities. The editor module is excluded from game targets.

Rigid Chest import requires an existing fitted rigid template for the selected character. The current Zeus configuration has no such template; use compatible skinned armor, or complete the existing attachment-fit setup before importing rigid Chest pieces.
