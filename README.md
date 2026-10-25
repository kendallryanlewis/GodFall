# GodFall development

Manage armor and weapons in Unreal Editor. GitHub is version control; the editor and game do not download or admit GitHub item packs.

- Open **Tools → GodFall Equipment Manager** to import, edit, preview, remove/restore, and choose loot/free-store availability in one panel. See the [equipment guide](Plugins/GodFallPackRuntime/EQUIPMENT_MANAGER.md).
- Acquisition settings live in `/Game/GF/Items/DA_GF_EquipmentManager`. Armor definitions retain `/Game/GF/Items/Armor/Data/DA_GF_ArmorCatalog`; weapon definitions live in `/Game/GF/Weapons/Definitions`.
- Generic weapon pickups use enabled loot entries with **Random weapon pool** selected. Specialized pickups intersect their existing approved lists with the managed loot settings.
- The free store uses enabled **Free store** entries. New imports start as disabled drafts; removing an item disables acquisition while keeping source assets and owned-item metadata.
- Walk up to the Quartermaster in the Vertical Slice and press E (gamepad X / Square) to claim items free. No pause-menu store button.

Edit and save assets in Unreal, then commit/push source changes through version control. Pull source changes before opening the editor. No game executable or content-pack build is needed for ordinary editor asset changes. This repository contains plugin sources, the authored Equipment Manager settings and specialized Dagger pickup under `Content/GF`, and historical delivery artifacts. It is not a complete source-project checkout; the two authored assets depend on the existing GodFall_Clean project. Old cooked packs are not editable source assets.

`GameItems`, `GameContent`, and `launcher` retain historical distribution files. They are not the current development workflow. The automatic item-manifest workflow and the editor downloader have been removed. Previously published releases are unchanged snapshots and do not receive this editor-only update.
