"""Build the MIT guest adapter against this player's installed game jars in place.
Only mod classes and notices enter the output; no Minecraft classes/assets copied.
"""
import hashlib,json,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
rows=json.loads((root/'sheets/guest_exports.json').read_text());ids=set();java=['package dev.skycraft.client.render;','final class ClassicExportRules {']
for row in rows:
    assert set(row)=={'id','enabled','source'} and row['id'].isidentifier() and row['id'] not in ids and type(row['enabled']) is bool and row['source']
    ids.add(row['id']);java.append(f'    static final class {row["id"]} {{ static final boolean enabled={str(row["enabled"]).lower()}; }}')
assert ids=={'Arrows','CustomGeometry','Items','PlayerBody'},'Unresolved export rule references'
java.append('}')
folder=root/'guest/src/dev/skycraft/client/render';folder.mkdir(parents=True,exist_ok=True)
(folder/'ClassicExportRules.java').write_text('\n'.join(java)+'\n')
original=root/'upstream/SkyCraft/fabric/src/client/java/dev/skycraft/client/render/AvatarExporter.java'
source=original.read_text()
before='e instanceof ItemEntity || e instanceof AbstractArrow || e instanceof ItemSupplier'
after='(e instanceof ItemEntity && !ClassicExportRules.Items.enabled) || (e instanceof AbstractArrow && !ClassicExportRules.Arrows.enabled) || e instanceof ItemSupplier'
assert source.count(before)==1;source=source.replace(before,after)
assert source.count('if (e == player ||')==1
source=source.replace('if (e == player ||','if ((e == player && (!ClassicExportRules.PlayerBody.enabled || !camera.isDetached())) ||')
empty='public void submitCustomGeometry(PoseStack poseStack, RenderType renderType, SubmitNodeCollector.CustomGeometryRenderer customGeometryRenderer) {\n\t}'
method='''public void submitCustomGeometry(PoseStack poseStack, RenderType renderType, SubmitNodeCollector.CustomGeometryRenderer customGeometryRenderer) {
        if (!ClassicExportRules.CustomGeometry.enabled) return;
        Batch batch = this.batchFor(renderType);
        if (batch == null) return;
        this.capture.flush();
        this.capture.begin(batch);
        customGeometryRenderer.render(poseStack.last(), this.capture);
        this.capture.flush();
    }'''
assert source.count(empty)==1;source=source.replace(empty,method)
# Fabric injects overloads into Minecraft interfaces at runtime. Source-only
# Override annotations are omitted when compiling against the untouched game jar.
source=source.replace('@Override','')
(folder/'AvatarExporter.java').write_text(source)
classic=root/'guest/src/dev/skycraft/classic';classic.mkdir(parents=True,exist_ok=True)
rules=['package dev.skycraft.classic;','public final class ClassicRules {'];seen=set()
for r in json.loads((root/'sheets/pickup_rules.json').read_text()):
    assert set(r)=={'id','value','source'} and r['id'].isidentifier() and r['source'] and 0<float(r['value'])<=2.5 and r['id'] not in seen
    seen.add(r['id']);rules.append(f'public static final class {r["id"]} {{public static final double value={r["value"]};}}')
assert seen=={'Horizontal','Vertical'}
avatars=json.loads((root/'sheets/avatar_rules.json').read_text());assert len(avatars)==1
r=avatars[0];assert set(r)=={'id','texture','model','source'} and r['id']=='Steve' and r['model']=='WIDE' and r['source']
rules.append('public static final class Steve {public static final String texture='+json.dumps(r['texture'])+';}')
recovery=json.loads((root/'sheets/recovery_rules.json').read_text());assert len(recovery)==1
r=recovery[0];assert set(r)=={'id','request','item','preferredSlot','count','source'} and r['id']=='LostBow' and r['item']=='minecraft:bow' and r['count']==1 and 0<=r['preferredSlot']<9 and r['source'] and r['request']=='skycraft-classic/recover-bow.request'
rules.append('public static final class LostBow {public static final String request='+json.dumps(r['request'])+';public static final int slot='+str(r['preferredSlot'])+';}');rules.append('}')
(classic/'ClassicRules.java').write_text('\n'.join(rules)+'\n')
prism=root/'tools/prism';base=root/'private/guest-base/skycraft-fabric-0.1.2.jar'
if not base.exists():base=prism/'instances/SkyCraftClassic-QA1/.minecraft/mods/skycraft-fabric-0.1.2.jar'
assert base.exists(),'Original MIT guest jar required'
with zipfile.ZipFile(base) as z:assert json.loads(z.read('fabric.mod.json'))['version']=='0.1.2','Build input must be the pinned original MIT guest'
libs=list((prism/'libraries').rglob('*.jar'))
assert any(p.name=='minecraft-26.3-client.jar' for p in libs)
api=prism/'instances/SkyCraftClassic-QA1/.minecraft/mods'
apiJar=api/'fabric-api.jar';assert apiJar.exists();apiOut=root/'build/fabric-api-classpath';apiOut.mkdir(exist_ok=True)
with zipfile.ZipFile(apiJar) as z:
    for name in z.namelist():
        if name.startswith('META-INF/jars/') and name.endswith('.jar'):
            p=apiOut/Path(name).name;p.write_bytes(z.read(name));libs.append(p)
cp=';'.join(str(p) for p in [base,*libs])
out=root/'build/guest-classes-classic2';out.mkdir(exist_ok=True)
javac=next((root/'tools/jdk25').glob('*/bin/javac.exe'))
print('Guest export sheet preflight clean; using installed Minecraft jar as read-only compiler input.',flush=True)
subprocess.run([str(javac),'--release','25','-proc:none','-cp',cp,'-d',str(out),str(folder/'ClassicExportRules.java'),str(folder/'AvatarExporter.java'),*[str(p) for p in classic.rglob('*.java')]],check=True)
replacements={p.relative_to(out).as_posix():p.read_bytes() for p in out.rglob('*.class')}
dest=root/'build/skycraft-classic-guest-0.1.2-classic.1.jar'
with zipfile.ZipFile(base) as originalJar,zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
    for info in originalJar.infolist():
        if info.filename in replacements:continue
        data=originalJar.read(info.filename)
        if info.filename=='fabric.mod.json':
            metadata=json.loads(data);metadata['version']='0.1.2-classic.2';metadata['name']='SkyCraft Classic bridge (prototype)';metadata['entrypoints']['main'].append('dev.skycraft.classic.ClassicGameplay');metadata['mixins'].append('classic-client.mixins.json');data=json.dumps(metadata,indent=2).encode()
        z.writestr(info.filename,data)
    for name,data in replacements.items():z.writestr(name,data)
    z.writestr('LICENSE_SkyCraftClassic', (root/'LICENSE').read_bytes())
    z.writestr('classic-client.mixins.json',json.dumps({'required':True,'package':'dev.skycraft.classic.mixin','compatibilityLevel':'JAVA_25','mixins':['ItemGroundMixin'],'client':['SteveSkinMixin'],'injectors':{'defaultRequire':1}}))
with zipfile.ZipFile(dest) as z:
    assert not any(n.startswith(('net/minecraft/','assets/minecraft/')) for n in z.namelist())
report={'baseSha256':hashlib.sha256(base.read_bytes()).hexdigest(),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'compiledClasses':len(replacements),'containsMinecraftContent':False,'testedInGame':False}
(root/'build/guest-patch-evidence.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
