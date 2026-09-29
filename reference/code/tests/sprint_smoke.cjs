// Numerical regression tests; the same checks also run with real Three.js.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const modules=vm.runInNewContext(fs.readFileSync(root+'/js/loader.js','utf8').match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const f of [...modules,'tests/sprint_checks.js'])vm.runInThisContext(fs.readFileSync(root+'/'+f,'utf8'),{filename:f});
const report={environment:'Node + numeric_math.cjs; NOT Three.js/WebGL',checks:RTS.SprintChecks.run()};
fs.mkdirSync(root+'/tests/out',{recursive:true});fs.writeFileSync(root+'/tests/out/sprint_numeric.json',JSON.stringify(report,null,2));
for(const c of report.checks)console.log(c.ok?'PASS':'FAIL',c.name,c.ok?JSON.stringify(c.details):c.error);
if(report.checks.some(c=>!c.ok))process.exitCode=1;
