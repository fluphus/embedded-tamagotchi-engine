Import('env')
from pathlib import Path
probe=env.GetProjectOption('custom_hw_probe')
assert probe in ('pressure','offline')
env.BuildSources('$BUILD_DIR/hardware_probe',str(Path(env.subst('$PROJECT_DIR'))/'tests/hardware/probes'),src_filter=['+<'+probe+'.cpp>'])
if probe=='offline':env.Append(LINKFLAGS=['-Wl,--wrap=_Z4loopv'])
