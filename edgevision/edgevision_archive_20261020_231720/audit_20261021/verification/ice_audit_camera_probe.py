import json, os, pathlib, subprocess
env=dict(os.environ,LD_LIBRARY_PATH='/tmp/ice_slim_deps/root/usr/lib/x86_64-linux-gnu:/tmp/ice_slim_sdk')
logs=[]
for flags in [[],['--channel','1'],['--channel','0'],['--simulate-context','--angle','1','--working','1'],['--width','1280garbage']]:
    args=['/tmp/ice_scope_camera_sim','--task','/tmp/ice_scope_camera_ch1.yaml','--frames','3']+flags
    if not flags:args+=['--out','/tmp/ice_audit_camera_artifacts/run']
    r=subprocess.run(args,capture_output=True,text=True,env=env)
    logs.append(f'flags={flags} exit={r.returncode}\n{r.stderr}{r.stdout}')
    if flags==['--channel','0']:assert r.returncode==2 and 'differs from task' in r.stderr
    elif flags==['--width','1280garbage']:assert r.returncode==2 and 'invalid integer' in r.stderr
    elif flags and flags[0]=='--simulate-context':
        rows=[json.loads(x) for x in r.stdout.splitlines()]
        assert r.returncode==0 and len(rows)==3 and all(not x['api_called'] for x in rows)
    else:
        rows=[json.loads(x) for x in r.stdout.splitlines()]
        assert r.returncode==0 and 'context_source=direct' in r.stderr
        assert [x['alarm'] for x in rows]==[0,1,0]
        assert all(x['camera_channel']==1 and x['api_called'] for x in rows)
        if not flags:assert all(pathlib.Path(f'/tmp/ice_audit_camera_artifacts/run_{i}_overlay.ppm').is_file() for i in range(3))
pathlib.Path('/tmp/ice_audit_camera_probe.log').write_text('\n'.join(logs))
print('Camera simulated NV12 main and artifacts: PASS')
