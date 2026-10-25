"""Bundle a packaged Windows build for immutable GitHub release delivery."""
from __future__ import annotations
import argparse, hashlib, json, re, zipfile
from pathlib import Path

CHUNK=768*1024*1024
SKIP_DIRS={'Saved','GodFallGitHub','GodFallGitHubSync'}
SKIP_NAMES={'Play-GodFall.cmd','Start-GodFall.ps1','Start-GodFallRelease.ps1'}

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(4*1024*1024),b''):h.update(block)
    return h.hexdigest()

def build(source:Path,output:Path,tag:str,revision:str):
    source=source.resolve();output=output.resolve()
    if source==output or source in output.parents:raise ValueError('Output must be outside source')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,79}',tag):raise ValueError('Invalid release tag')
    if not re.fullmatch(r'[a-f0-9]{40}',revision):raise ValueError('Full source revision required')
    if not (source/'GodFall_Clean.exe').is_file() or not (source/'GodFall_Clean/Content/Paks').is_dir():raise ValueError('Packaged Windows root required')
    if list(source.glob('*.uproject')):raise ValueError('Source projects are not release inputs')
    output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):raise ValueError('Release output must be new/empty')
    files=[];seen=set()
    for path in sorted(source.rglob('*')):
        relative=path.relative_to(source)
        if path.is_symlink():raise ValueError('Linked build entries are unsupported')
        if any(part in SKIP_DIRS for part in relative.parts):continue
        if not path.is_file() or path.name in SKIP_NAMES or path.suffix.lower() in ('.pdb','.sav'):continue
        if path.name.startswith('Manifest_'):continue
        name=relative.as_posix()
        if any(part in ('','..','.') for part in relative.parts) or ':' in name or name.casefold() in seen:raise ValueError('Unsafe/duplicate path')
        seen.add(name.casefold());files.append({'path':name,'size':path.stat().st_size,'sha256':digest(path)})
    archive=output/'GodFall-Windows.zip'
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_STORED,allowZip64=True) as bundle:
        for entry in files:bundle.write(source/entry['path'],entry['path'])
    parts=[]
    with archive.open('rb') as stream:
        index=1
        while True:
            block=stream.read(4*1024*1024)
            if not block:break
            part=output/f'GodFall-Windows.zip.part{index:03d}';h=hashlib.sha256();size=0
            with part.open('wb') as target:
                while block:
                    target.write(block);h.update(block);size+=len(block)
                    if size==CHUNK:break
                    block=stream.read(min(4*1024*1024,CHUNK-size))
            parts.append({'name':part.name,'size':size,'sha256':h.hexdigest()});index+=1
    manifest={'schema_version':1,'project':'GodFall_Clean','platform':'Win64','engine':'5.8',
              'release_tag':tag,'source_commit':revision,'executable':'GodFall_Clean.exe',
              'archive':{'size':archive.stat().st_size,'sha256':digest(archive),'parts':parts},'files':files}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    launcher=Path(__file__).resolve().parents[1]/'launcher'
    with zipfile.ZipFile(output/'GodFall-Launcher.zip','w',compression=zipfile.ZIP_DEFLATED) as bootstrap:
        for name in ('Play-GodFall.cmd','Start-GodFallRelease.ps1','Start-GodFall.ps1'):
            bootstrap.write(launcher/name,name)
        bootstrap.writestr('HOW-TO-PLAY.txt','For the downloaded ZIP: Properties > Unblock > Apply if Windows shows that option, then Extract All.\r\nKeep these three launcher files together. Open Play-GodFall.cmd.\r\nThe first launch downloads all game content from GitHub. Later launches check for updates.\r\nThe game installs under %LOCALAPPDATA%\\GodFallGitHub; PlayerData is kept outside version folders.\r\nIn the Vertical Slice, open the pause menu and choose FREE STORE. Claims cost no currency.\r\n')
    print(json.dumps({'files':len(files),'bytes':manifest['archive']['size'],'parts':len(parts),'tag':tag}))
    return manifest

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--tag',required=True);parser.add_argument('--revision',required=True)
    args=parser.parse_args();build(args.source,args.output,args.tag,args.revision)
