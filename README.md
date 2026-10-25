# GodFall development

Manage armor and weapons in Unreal Editor. GitHub is version control; the editor and game do not download or admit GitHub item packs.

- Armor definitions are managed in `/Game/GF/Items/Armor/Data/DA_GF_ArmorCatalog`.
- Generic weapon loot uses `ApprovedDefinitionClasses` on `/Game/GF/Weapons/Pickups/BP_GF_WeaponPickup`. Specialized pickups retain their own approved lists.
- The free store uses the local Sword and Dagger definition Blueprints in `/Game/GF/Weapons/Definitions`, plus the authored armor catalog. Existing weapon membership is preserved; adding another weapon to store membership currently requires updating `GetFreeStoreDefinitions`.
- Walk up to the Quartermaster in the Vertical Slice and press E (gamepad X / Square) to claim items free. No pause-menu store button.

Edit and save assets in Unreal, then commit/push source changes through version control. Pull source changes before opening the editor. No game executable or content-pack build is needed for ordinary editor asset changes. This repository contains the plugin sources and historical delivery artifacts; its old cooked packs are not editable source assets or a complete source-project checkout.

`GameItems`, `GameContent`, and `launcher` retain historical distribution files. They are not the current development workflow. The automatic item-manifest workflow and the editor downloader have been removed. Previously published releases are unchanged snapshots and do not receive this editor-only update.
