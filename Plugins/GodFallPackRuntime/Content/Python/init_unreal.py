"""Once per editor launch: validate/download packs, then mount an isolated cooked cache."""
import unreal

# Cooking and automation commandlets must never fetch or include the editor cache.
if unreal.GodFallPackLibrary.is_editor_pack_sync_allowed():
    import godfall_editor_packs
    import re
    override=re.search(r'-GodFallPackTestRepository=("[^"]+"|\S+)',unreal.SystemLibrary.get_command_line())
    godfall_editor_packs.start(override.group(1).strip('"') if override else None)
