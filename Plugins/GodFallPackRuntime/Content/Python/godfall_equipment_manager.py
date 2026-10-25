"""Equipment Manager editor actions. All assets stay in the Clean Unreal project."""
import datetime
import hashlib
import json
import re
import shutil
import sys
from contextlib import contextmanager
from pathlib import Path
import unreal

WORKSPACE=Path(r'C:\Users\June\Documents\ChatGPT\GodFall')
PROJECT=Path(r'C:\Users\June\Documents\Unreal Projects\GodFall_Clean')
TASK='20261025-equipment-manager'
BACKUP=WORKSPACE/'backups'/TASK/'prechange'/'authoring'
OUT=WORKSPACE/'artifacts'/TASK
WEAPON_ROOT='/Game/GF/Weapons/Definitions/'
SWORD=WEAPON_ROOT+'BP_GF_WeaponDefinition_TestSword'

def _guard():
    if Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()!=PROJECT/'GodFall_Clean.uproject':
        raise RuntimeError('Equipment Manager is restricted to GodFall_Clean.')
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop Play before editing equipment.')

def _maps():
    sys.path.insert(0,str(WORKSPACE/'tools'))
    import ue_blueprint_map_lib
    return ue_blueprint_map_lib

def _backup(asset):
    path=asset.get_path_name().split('.')[0]
    if not path.startswith('/Game/'):
        raise RuntimeError('Only project-owned equipment can be edited.')
    source=PROJECT/'Content'/(path[6:]+'.uasset')
    dest=BACKUP/(path[6:]+'.uasset')
    if source.is_file() and not dest.exists():
        dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)

def _record(action,targets,operation):
    _guard()
    report=dict(schema_version='1.0',task_id=TASK,request='Manage Unreal armor and weapons from one panel.',outcome=action,success=[action],status='failed',observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),targets=targets,changes=[],checks=[],errors=[],backup_set=str(BACKUP))
    try:
        result=operation()
        report['status']='complete'
        report['checks'].append(dict(name=action,result='pass',source='live_editor'))
        return result
    except Exception as error:
        report['errors'].append(str(error));unreal.log_error(str(error));raise
    finally:
        OUT.mkdir(parents=True,exist_ok=True)
        (OUT/('action-'+re.sub(r'[^a-z0-9]+','-',action.lower())+'.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

def _refresh():
    error=unreal.GodFallEquipmentEditorLibrary.refresh_catalog()
    if error!='':raise RuntimeError(error or 'Catalog refresh failed. Resolve invalid or conflicting equipment definitions.')

@contextmanager
def _armor_compatibility():
    """Preserve one existing Belt authored in a legacy Pants folder; never repair it implicitly."""
    import godfall_wearables as wearable
    previous=wearable.KNOWN_PREEXISTING_CATALOG_REJECTS
    allowed=list(previous)
    path='/Game/GF/Items/Armor/Zeus/Pants/Belt/Data/DA_GF_Zeus_Belt_Belt_Pants'
    belt=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    fields={'DefinitionId':'GF_Zeus_Belt_Belt_Pants','CharacterId':'Zeus','EquipmentSlotId':'Belt','ItemCategoryId':'Armor','TemplateRowId':'E_Legs_002'}
    if belt and all(str(belt.get_editor_property(k))==v for k,v in fields.items()):
        mesh=belt.get_editor_property('PresentationMesh')
        if mesh and mesh.get_path_name()=='/Game/GF/Items/Armor/Zeus/Pants/Belt/Meshes/Belt.Belt' and belt.get_editor_property('PresentationStaticMesh') is None:
            signature={'path':path,'id':fields['DefinitionId'],'reasons':['equipment region requires EquipmentSlotId=Legs']}
            if signature not in allowed:allowed.append(signature)
    wearable.KNOWN_PREEXISTING_CATALOG_REJECTS=allowed
    try:yield
    finally:wearable.KNOWN_PREEXISTING_CATALOG_REJECTS=previous

def prepare_edit(path):
    """Called before details editing; capture source and reconcile prior manual changes."""
    _guard()
    asset=unreal.load_asset(path)
    if not asset:return
    _backup(asset)
    if isinstance(asset,unreal.Blueprint):
        maps=_maps();audit=maps.audit_assets([path.split('.')[0]])
        if audit['status']!='no_change':maps.sync_assets([path.split('.')[0]])
        map_path=WORKSPACE/'clean-blueprint-maps'/(path.split('.')[0][1:]+'.md')
        text=map_path.read_text(encoding='utf-8')
        plan='- Equipment Manager: edit data-only definition defaults; preserve stable DefinitionId/WeaponClassId, graphs, callers and saved-item identity. Compile/save, synchronize map and audit after Save changes.\n'
        if plan not in text:map_path.write_text(text.replace('<!-- PLANNED-CHANGES:START -->\n','<!-- PLANNED-CHANGES:START -->\n'+plan),encoding='utf-8')

def save_definition(path):
    def save():
        asset=unreal.load_asset(path)
        if not asset:raise RuntimeError('Select a valid definition.')
        _backup(asset)
        if isinstance(asset,unreal.Blueprint):
            unreal.BlueprintEditorLibrary.compile_blueprint(asset)
            if str(asset.get_editor_property('status')).endswith('ERROR'):raise RuntimeError('Definition failed to compile.')
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False):raise RuntimeError('Definition could not be saved.')
        if isinstance(asset,unreal.Blueprint):
            maps=_maps();maps.sync_assets([path.split('.')[0]])
            if maps.audit_assets([path.split('.')[0]])['status']!='no_change':raise RuntimeError('Definition map audit failed.')
        _refresh()
        return asset
    return _record('Save definition',[path],save)


def _delete_plan(path):
    """Read-only preflight. Never force-clear gameplay or shared-asset references."""
    _guard()
    if unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('Save your current asset and level edits before deleting equipment.')
    path=path.split('.')[0]
    asset=unreal.load_asset(path)
    if not asset:raise RuntimeError('The selected item no longer exists. Refresh the list.')
    weapon=isinstance(asset,unreal.Blueprint)
    definition=unreal.get_default_object(asset.generated_class()) if weapon else asset
    policy=unreal.load_asset('/Game/GF/Items/DA_GF_EquipmentManager')
    catalog=unreal.load_asset('/Game/GF/Items/Armor/Data/DA_GF_ArmorCatalog')
    entries=list(policy.get_editor_property('items'))
    matching=[e for e in entries if path+'.' in e.export_text()]
    if len(matching)!=1:raise RuntimeError('Select exactly one registered equipment definition.')
    prefix=str(definition.get_editor_property('WeaponClassId' if weapon else 'DefinitionId'))
    if prefix in ('','None') or '@' in prefix:raise RuntimeError('The item has no safe stable identity.')
    others=list(catalog.get_editor_property('Definitions'))
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path('/Game/GF/Weapons/Definitions',recursive=True):
        obj=data.get_asset()
        if isinstance(obj,unreal.Blueprint) and obj.generated_class():others.append(unreal.get_default_object(obj.generated_class()))
    for other in others:
        if other==definition:continue
        try:other_prefix=str(other.get_editor_property('WeaponClassId'))
        except Exception:other_prefix=str(other.get_editor_property('DefinitionId'))
        if other_prefix==prefix:raise RuntimeError('Another item shares this saved identity. Give the items distinct stable IDs before deleting either one.')
    allowed={policy.get_path_name().split('.')[0],catalog.get_path_name().split('.')[0]}
    def refs(p):return {str(x).split('.')[0] for x in unreal.EditorAssetLibrary.find_package_referencers_for_asset(p,True)}-{p}
    blockers=refs(path)-allowed
    if blockers:raise RuntimeError('This definition is used directly by these assets. Remove those references first:\n'+'\n'.join(sorted(blockers)))
    if weapon:
        if not path.startswith(WEAPON_ROOT):raise RuntimeError('Only managed weapon definitions can be deleted.')
        owner='/Game/GF/Weapons/Imported/'
        maps=_maps()
        if maps.audit_assets([path])['status']!='no_change':raise RuntimeError('Definition map is stale. Reconcile it before deletion.')
    else:
        parts=path.split('/')
        if len(parts)<10 or parts[1:5]!=['Game','GF','Items','Armor']:raise RuntimeError('Armor must be in its canonical character/region/piece folder.')
        owner='/'.join(parts[:8])+'/'
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_searchable_names=False,include_soft_management_references=False,include_hard_management_references=False)
    candidates={path};pending=[path]
    while pending:
        for dependency in registry.get_dependencies(pending.pop(),options):
            dependency=str(dependency)
            if dependency.startswith(owner) and dependency not in candidates and unreal.EditorAssetLibrary.does_asset_exist(dependency):
                # Only art dependencies, never another definition or gameplay asset.
                obj=unreal.load_asset(dependency)
                if isinstance(obj,(unreal.StaticMesh,unreal.SkeletalMesh,unreal.MaterialInterface,unreal.Texture)):
                    candidates.add(dependency);pending.append(dependency)
    changed=True
    while changed:
        shared={p for p in candidates-{path} if refs(p)-candidates}
        changed=bool(shared);candidates-=shared
    files={p:PROJECT/'Content'/(p[6:]+'.uasset') for p in candidates}
    if not all(f.is_file() for f in files.values()):raise RuntimeError('Save all selected source files before deletion.')
    return dict(path=path,asset=asset,definition=definition,weapon=weapon,prefix=prefix,policy=policy,catalog=catalog,entries=entries,files=files)


def delete_item(path,confirmed=False,show_dialog=True):
    """Permanent, backed-up definition deletion; old saves use the retirement ledger."""
    task='20261025-equipment-delete'
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    report_path=WORKSPACE/'artifacts'/task/('delete-'+stamp+'.json')
    backup=WORKSPACE/'backups'/task/'prechange/actions'/stamp
    report=dict(schema_version='1.0',task_id=task,request='Permanently delete the selected equipment.',outcome='Remove the definition and unshared art; clear saved equipped copies on next Play.',success=['Reference-safe backed-up deletion','Unregister acquisition','Retire saved identity'],status='failed',observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),targets=[path],changes=[],checks=[],errors=[],backup_set=str(backup))
    policy_before=None;catalog_before=None;retired_before=None;deleted=False
    try:
        plan=_delete_plan(path)
        files=plan['files'];path=plan['path'];policy=plan['policy'];catalog=plan['catalog']
        if show_dialog:
            confirmed=unreal.EditorDialog.show_message('Delete equipment permanently',f'Delete {plan["definition"].get_name()}?\n\nThe definition and {len(files)-1} unshared art asset(s) will be removed. Shared assets stay. Saved copies will be removed and unequipped on next Play / Continue.\n\nA recoverable backup will be created first. This cannot be restored with the Disable / restore button.',unreal.AppMsgType.YES_NO)==unreal.AppReturnType.YES
        if not confirmed:
            report['status']='no_change';report['checks'].append(dict(name='Cancelled without mutation',result='pass',source='live_editor'));return report
        plan=_delete_plan(path) # Recheck after the confirmation dialog.
        files=plan['files'];policy_before=list(policy.get_editor_property('items'));catalog_before=list(catalog.get_editor_property('Definitions'));retired_before=list(policy.get_editor_property('retired_item_prefixes'))
        copy_files=set(files.values())|{PROJECT/'Content/GF/Items/DA_GF_EquipmentManager.uasset',PROJECT/'Content/GF/Items/Armor/Data/DA_GF_ArmorCatalog.uasset'}
        copy_files.update(sidecar for f in list(copy_files) for suffix in ('.uexp','.ubulk','.uptnl') if (sidecar:=f.with_suffix(suffix)).is_file())
        copy_files.update((PROJECT/'Saved/SaveGames').glob('*.sav'))
        assert not backup.exists()
        identities={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in copy_files}
        for f in copy_files:
            target=Path(chr(92)*2+'?'+chr(92)+str(backup/f.relative_to(PROJECT)))
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target)
            if hashlib.sha256(target.read_bytes()).hexdigest()!=identities[str(f)]:raise RuntimeError('Backup verification failed. No asset deletion attempted.')
        report['checks'].append(dict(name='Immutable file backups verified',result='pass',source='reload',details=json.dumps(identities)))
        unreal.GodFallEquipmentEditorLibrary.select_definition(None)
        policy.set_editor_property('retired_item_prefixes',list(dict.fromkeys(retired_before+[unreal.Name(plan['prefix'])])))
        policy.set_editor_property('items',[e for e in policy_before if path+'.' not in e.export_text()])
        if not plan['weapon']:catalog.set_editor_property('Definitions',[d for d in catalog_before if d.get_path_name().split('.')[0]!=path])
        if not unreal.EditorAssetLibrary.save_loaded_asset(policy,False) or (not plan['weapon'] and not unreal.EditorAssetLibrary.save_loaded_asset(catalog,False)):
            raise RuntimeError('Could not save deletion settings; no source deletion attempted.')
        unreal.EditorAssetLibrary.delete_asset(path)
        deleted=not unreal.EditorAssetLibrary.does_asset_exist(path)
        if not deleted:raise RuntimeError('Unreal could not delete the definition; acquisition settings will be restored.')
        report['changes'].append(dict(target=path,summary='Permanently deleted the backed-up definition and retired its saved identity.'))
        # Art cleanup is subordinate to the committed item deletion. Recheck each
        # asset after removing its users; never force-delete a newly shared asset.
        pending=set(files)-{path};retained=set()
        while pending:
            removable=[p for p in pending if not unreal.EditorAssetLibrary.find_package_referencers_for_asset(p,True)]
            if not removable:break
            for p in removable:
                unreal.EditorAssetLibrary.delete_asset(p);pending.remove(p)
                if not unreal.EditorAssetLibrary.does_asset_exist(p):report['changes'].append(dict(target=p,summary='Deleted backed-up, unshared art dependency.'))
                else:retained.add(p)
        retained.update(pending)
        if retained:report['checks'].append(dict(name='Art retained because Unreal could not safely delete it',result='pass',source='live_editor',details='\n'.join(sorted(retained))))
        if plan['weapon']:
            maps=_maps();manifest=json.loads(Path(maps.MANIFEST_PATH).read_text());manifest['blueprints']=[e for e in manifest['blueprints'] if e['asset_path']!=path]
            map_path=Path(maps._map_path(path))
            if map_path.exists():shutil.copy2(map_path,backup/'definition-map.md');map_path.unlink()
            maps._write_json(maps.MANIFEST_PATH,manifest);maps._write_index(manifest['blueprints'])
        _refresh()
        assert not unreal.EditorAssetLibrary.does_asset_exist(path)
        assert not any(path+'.' in e.export_text() for e in policy.get_editor_property('items'))
        assert unreal.Name(plan['prefix']) in policy.get_editor_property('retired_item_prefixes')
        report['checks'].append(dict(name='Definition absent and saved identity retired',result='pass',source='live_editor'))
        report['status']='complete'
        if show_dialog:
            retained_note=('\n\nThese art files were retained:\n'+'\n'.join(sorted(retained))) if retained else ''
            unreal.EditorDialog.show_message('Equipment deleted',f'Deleted. Saved copies will be removed and unequipped on next Play / Continue.\n\nBackup: {backup}'+retained_note,unreal.AppMsgType.OK)
        return report
    except Exception as error:
        report['errors'].append(str(error))
        if policy_before is not None and not deleted:
            try:
                policy.set_editor_property('items',policy_before)
                if list(policy.get_editor_property('retired_item_prefixes'))!=retired_before:policy.set_editor_property('retired_item_prefixes',retired_before)
                if not plan['weapon']:catalog.set_editor_property('Definitions',catalog_before)
                if not unreal.EditorAssetLibrary.save_loaded_asset(policy,False):raise RuntimeError('Could not save restored acquisition settings.')
                if not plan['weapon'] and not unreal.EditorAssetLibrary.save_loaded_asset(catalog,False):raise RuntimeError('Could not save restored armor catalog.')
            except Exception as rollback:report['errors'].append('Rollback: '+str(rollback))
        if show_dialog:unreal.EditorDialog.show_message('Delete stopped',str(error),unreal.AppMsgType.OK)
        raise
    finally:
        report_path.parent.mkdir(parents=True,exist_ok=True);report_path.write_text(json.dumps(report,indent=2)+'\n')

def import_armor(filenames=None, **options):
    _guard()
    import godfall_wearables_import
    with _armor_compatibility():
        result=godfall_wearables_import.run_import_wearables(fbx_paths=filenames, **options)
    _refresh()
    if result and result.get('status') in ('failed','blocked'):
        raise RuntimeError('Armor import needs attention: '+'; '.join(result.get('errors',[])))
    return result

def add_weapon(mesh,template_path=SWORD):
    """Register an imported static mesh using an existing combat-style template."""
    _guard()
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Weapons currently use Static Mesh presentation. Select an imported Static Mesh.')
    name=re.sub(r'[^A-Za-z0-9_]','_',mesh.get_name())
    path=WEAPON_ROOT+'BP_GF_WeaponDefinition_'+name
    def create():
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            existing=unreal.load_asset(path);cdo=unreal.get_default_object(existing.generated_class())
            if cdo.get_editor_property('PresentationStaticMesh')!=mesh:raise RuntimeError('A different weapon already owns this name. Rename the imported mesh.')
            _refresh();unreal.GodFallEquipmentEditorLibrary.select_definition(cdo);return existing
        template=unreal.load_asset(template_path)
        if not isinstance(template,unreal.Blueprint):raise RuntimeError('Choose an existing weapon definition template.')
        maps=_maps()
        if maps.audit_assets([template_path])['status']!='no_change':raise RuntimeError('Template map is stale. Reconcile it before authoring.')
        new=unreal.EditorAssetLibrary.duplicate_asset(template_path,path)
        if not new:raise RuntimeError('Could not duplicate weapon template.')
        # A new Blueprint receives its first live map before any default is changed.
        maps.sync_assets([path]);prepare_edit(path)
        registered=False
        try:
            cdo=unreal.get_default_object(new.generated_class())
            cdo.set_editor_property('DefinitionId',unreal.Name('GF_Weapon_'+name))
            cdo.set_editor_property('WeaponClassId',unreal.Name('Weapon_'+name))
            cdo.set_editor_property('PresentationStaticMesh',mesh)
            cdo.set_editor_property('PresentationLocation',unreal.Vector(0,0,0))
            cdo.set_editor_property('PresentationRotation',unreal.Rotator(0,0,0))
            cdo.set_editor_property('PresentationScale',unreal.Vector(1,1,1))
            unreal.BlueprintEditorLibrary.compile_blueprint(new)
            if not unreal.EditorAssetLibrary.save_loaded_asset(new,only_if_is_dirty=False):raise RuntimeError('Could not save the new weapon.')
            maps.sync_assets([path])
            if maps.audit_assets([path])['status']!='no_change':raise RuntimeError('New definition map audit failed.')
            _refresh()
            registered=True
            unreal.GodFallEquipmentEditorLibrary.select_definition(unreal.get_default_object(new.generated_class()))
            return new
        except Exception:
            # Keep imported source art. Remove only this exact newly-owned definition.
            if not registered:unreal.EditorAssetLibrary.delete_asset(path)
            raise
    return _record('Add weapon',[mesh.get_path_name(),path],create)

def import_weapon(filenames=None):
    _guard()
    destination='/Game/GF/Weapons/Imported'
    assets=[]
    if filenames is None:
        assets=unreal.AssetToolsHelpers.get_asset_tools().import_assets_with_dialog(destination)
    else:
        tasks=[]
        for filename in filenames:
            if not Path(filename).is_file():raise RuntimeError('Import source does not exist: '+filename)
            name=re.sub(r'[^A-Za-z0-9_]','_',Path(filename).stem)
            if unreal.EditorAssetLibrary.does_asset_exist(destination+'/'+name):raise RuntimeError('Import would overwrite an existing mesh: '+name)
            task=unreal.AssetImportTask();task.filename=filename;task.destination_path=destination;task.destination_name=name
            task.automated=True;task.replace_existing=False;task.save=True
            options=unreal.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.import_animations=False
            options.import_materials=False;options.import_textures=False
            options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;options.automated_import_should_detect_type=False
            task.options=options;tasks.append(task)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        assets=[unreal.load_asset(p) for task in tasks for p in task.imported_object_paths]
    return [add_weapon(asset) for asset in assets if isinstance(asset,unreal.StaticMesh)]

def add_selected():
    _guard();selected=unreal.EditorUtilityLibrary.get_selected_assets()
    if len(selected)!=1:raise RuntimeError('Select exactly one imported mesh or equipment definition in the Content Browser.')
    asset=selected[0];path=asset.get_path_name()
    if isinstance(asset,(unreal.StaticMesh,unreal.SkeletalMesh)):
        if '/Items/Armor/' in path:
            import godfall_wearables
            with _armor_compatibility():
                result=godfall_wearables.run_add_wearable(source_asset_path=path)
            _refresh();return result
        return add_weapon(asset)
    _refresh()
    definition=unreal.get_default_object(asset.generated_class()) if isinstance(asset,unreal.Blueprint) else asset
    if not unreal.GodFallEquipmentEditorLibrary.select_definition(definition):
        raise RuntimeError('Select an armor definition in the armor catalog or a weapon definition under GF/Weapons/Definitions.')

def locate(path):
    _guard()
    if path:unreal.EditorAssetLibrary.sync_browser_to_objects([path])

def preview_fit():
    _guard()
    import godfall_attachment_preview
    godfall_attachment_preview.preview()

def approve_fit():
    _guard()
    import godfall_attachment_preview
    with _armor_compatibility():
        godfall_attachment_preview.menu('approve')
    _refresh()
