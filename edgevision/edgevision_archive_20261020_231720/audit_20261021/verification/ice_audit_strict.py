import json, pathlib, shlex, subprocess
root=pathlib.Path.cwd()
files=subprocess.check_output(['git','ls-files','-z','-m','-o','--exclude-standard']).decode().split('\0')
entries=[]
for build in ['/tmp/ice_scope_camera','/tmp/ice_slim_rknn']:
    entries+=json.loads(pathlib.Path(build,'compile_commands.json').read_text())
log=[]
for name in sorted(set(files)):
    if not name.endswith('.cpp'):continue
    entry=next(x for x in entries if pathlib.Path(x['file']).resolve()==root/name)
    args=shlex.split(entry['command']);clean=[];i=0
    while i<len(args):
        if args[i]=='-o':i+=2;continue
        if args[i]=='-c':i+=1;continue
        clean.append(args[i]);i+=1
    clean+=['-Wall','-Wextra','-Werror','-fsyntax-only']
    run=subprocess.run(clean,cwd=entry['directory'],capture_output=True,text=True)
    log.append(f'{name}: {run.returncode}\n{run.stdout}{run.stderr}')
    pathlib.Path('/tmp/ice_audit_strict.log').write_text('\n'.join(log))
    if run.returncode:raise SystemExit(run.returncode)
print('Strict syntax PASS:',len(log),'sources')
