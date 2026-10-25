"""Editor companion to the packaged launcher. Downloaded files never enter source Content."""
import csv
import hashlib
import io
import json
import msvcrt
import os
import re
import shutil
import subprocess
from pathlib import Path, PurePosixPath
import unreal

OWNER = 'GodFallEditorExtract-v1'
PACK_PREFIX = '../../../GodFall_Clean/Content/'
MARKER = '.godfall-editor-cache.json'

def safe(path, root):
    path=Path(os.path.abspath(str(path)));root=Path(os.path.abspath(str(root)))
    if path!=root and root not in path.parents: raise RuntimeError('Editor cache path escapes its owned root')
    for part in [path,*path.parents]:
        if part.exists() and part.stat(follow_symlinks=False).st_file_attributes & 0x400:
            raise RuntimeError('Linked editor cache paths are unsupported')
    return path

def sha(path):
    value=hashlib.sha256()
    with path.open('rb') as handle:
        for block in iter(lambda:handle.read(1024*1024),b''):value.update(block)
    return value.hexdigest()

def decode(data):
    return data.decode('utf-16' if data.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')

def run(args,timeout=180):
    result=subprocess.run([str(a) for a in args],capture_output=True,timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW)
    output=decode(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError('Content helper failed: '+output[-1500:])
    return output

def remove_owned(path,root):
    safe(path,root)
    record=path/MARKER
    if not record.is_file() or json.loads(record.read_text(encoding='utf-8')).get('owner')!=OWNER:
        raise RuntimeError('Refusing an unowned editor extraction directory')
    for child in path.rglob('*'):safe(child,root)
    shutil.rmtree(path)

def prepare_cache(root,manifest,pak_tool):
    extracted=safe(root/'Extracted',root);extracted.mkdir(exist_ok=True)
    digest=hashlib.sha256(json.dumps(manifest,sort_keys=True).encode()).hexdigest()
    cache=safe(extracted/digest[:16],root)
    if cache.exists():
        record=json.loads((cache/MARKER).read_text(encoding='utf-8'))
        if record.get('owner')!=OWNER or record.get('manifest_sha256')!=digest:raise RuntimeError('Unowned extraction cache or hash-prefix collision')
        files=record.get('files',{})
        if record.get('complete') and files and all(safe(cache/name,cache).is_file() and sha(cache/name)==value for name,value in files.items()):
            return cache
        remove_owned(cache,root)
    cache.mkdir();(cache/MARKER).write_text(json.dumps({'owner':OWNER,'manifest_sha256':digest,'complete':False}),encoding='utf-8')
    seen=set();files={}
    for index,pack in enumerate(manifest['packs']):
        name=pack['id']+'_'+pack['sha256'][:16]+'_P.pak'
        pak=safe(root/'Paks/GodFallGitHub'/name,root)
        if pak.stat().st_size!=pack['size'] or sha(pak)!=pack['sha256']:raise RuntimeError('Cached pack integrity failed')
        csv_path=cache/('pack-'+str(index)+'.csv')
        listing=run([pak_tool,pak,'-List','-csv='+str(csv_path)])
        mounts=re.findall(r'Listing .* with mount point "([^"]+)"',listing)
        if len(mounts)!=1 or not mounts[0].startswith(PACK_PREFIX):raise RuntimeError('Pack mount must belong to GodFall_Clean Content')
        mount=mounts[0][len(PACK_PREFIX):]
        rows=list(csv.DictReader(io.StringIO(decode(csv_path.read_bytes())),skipinitialspace=True))
        if not rows or len(rows)>50000:raise RuntimeError('Invalid pack directory')
        names=[]
        for row in rows:
            filename=row['Filename'];logical=mount+filename
            if row['Deleted']!='false' or not re.fullmatch(r'(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\.(?:uasset|uexp|ubulk|uptnl)',filename):
                raise RuntimeError('Unsupported or unsafe cooked entry')
            if not re.fullmatch(r'GFExternal/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\.(?:uasset|uexp|ubulk|uptnl)',logical):
                raise RuntimeError('Pack must contain only namespaced GFExternal assets')
            if logical.lower() in seen:raise RuntimeError('Packs contain conflicting asset paths')
            seen.add(logical.lower());names.append((filename,logical))
        raw=cache/('raw-'+str(index));raw.mkdir()
        run([pak_tool,pak,'-Extract',raw])
        actual={p.relative_to(raw).as_posix() for p in raw.rglob('*') if p.is_file()}
        if actual!={name for name,_ in names}:raise RuntimeError('Extracted files differ from the validated pak index')
        for name,logical in names:
            source=safe(raw/Path(PurePosixPath(name)),raw)
            dest=safe(cache/'Content'/Path(PurePosixPath(logical)),cache)
            dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
            files[dest.relative_to(cache).as_posix()]=sha(dest)
        # Raw files are the exact validated extraction set inside this newly owned cache.
        safe(raw,cache);shutil.rmtree(raw);csv_path.unlink()
    (cache/MARKER).write_text(json.dumps({'owner':OWNER,'manifest_sha256':digest,'complete':True,'files':files},indent=2),encoding='utf-8')
    return cache

def start(repository_url=None):
    if getattr(unreal,'_gf_github_editor_started',False):return
    unreal._gf_github_editor_started=True
    root=None
    try:
        project=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()))
        if project.name!='GodFall_Clean.uproject':return
        root=safe(project.parent/'Saved/GodFallGitHubEditor',project.parent/'Saved')
        root.mkdir(parents=True,exist_ok=True)
        lock_path=safe(root/'editor.lock',root)
        lock=lock_path.open('a+b');lock.seek(0)
        try:msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1)
        except OSError:
            lock.close();raise RuntimeError('Another editor owns this project content cache; close it before testing GitHub packs')
        unreal._gf_github_editor_cache_lock=lock
        script=Path(__file__).resolve().parents[2]/'Resources/Start-GodFall.ps1'
        powershell=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
        args=[powershell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',script,'-EditorProject',project,'-SyncOnly','-AllowLocalFallback']
        if repository_url:args+=['-RepositoryBaseUrl',repository_url]
        try:unreal.log(run(args,timeout=300).strip())
        except Exception as error:unreal.log_warning('GitHub download unavailable; using local acquisition: '+str(error))
        record=root/'Paks/GodFallGitHub/.godfall-github.json'
        if not record.is_file():return
        owned=json.loads(record.read_text(encoding='utf-8-sig'))
        if owned.get('owner')!='GodFallGitHubPacks-v1':raise RuntimeError('Unknown cached pack owner')
        manifest=owned['manifest']
        if not manifest['packs']:
            extracted=safe(root/'Extracted',root)
            if extracted.exists():
                for old in extracted.iterdir():
                    if old.is_dir():remove_owned(old,root)
            unreal.log('GodFall GitHub: no packs returned; local armor and weapons supply loot and the store.')
            return
        pak_tool=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_dir()))/'Binaries/Win64/UnrealPak.exe'
        cache=prepare_cache(root,manifest,pak_tool)
        unreal.SystemLibrary.execute_console_command(None,'cook.AllowCookedDataInEditorBuilds 1')
        unreal.SystemLibrary.execute_console_command(None,'s.AllowUnversionedContentInEditor 1')
        content=(cache/'Content/GFExternal').as_posix()+'/'
        unreal.SystemLibrary.execute_console_command(None,'PackageName.RegisterMountPoint /Game/GFExternal/ "'+content+'"')
        unreal.log('GodFall GitHub: mounted '+str(len(manifest['packs']))+' validated packs for this editor session. Restart the editor to synchronize changes.')
        for old in (root/'Extracted').iterdir():
            if old.is_dir() and old!=cache:remove_owned(old,root)
    except Exception as error:
        unreal.log_warning('GodFall GitHub editor cache: '+str(error)+'; local armor and weapons remain available.')
        # No source packages are altered. The native registry rejects unmounted definitions.
