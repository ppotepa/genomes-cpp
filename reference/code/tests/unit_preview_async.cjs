// Unit-lab generation is cancellable and leaves the previous preview live until commit.
const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'..');global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
global.requestAnimationFrame=callback=>setTimeout(()=>callback(Date.now()),0);
const loader=fs.readFileSync(path.join(root,'js/loader.js'),'utf8'),modules=vmModules(loader);
for(const file of modules)vm.runInThisContext(fs.readFileSync(path.join(root,'js',file+'.js'),'utf8'),{filename:file});
vm.runInThisContext(fs.readFileSync(path.join(root,'js/ui/unitPreview.js'),'utf8'),{filename:'unitPreview.js'});
function vmModules(text){return vm.runInNewContext(text.match(/const local=(\[[\s\S]*?\]);/)[1]).map(f=>f.replace(/^js\//,'').replace(/\.js$/,''));}
const R=RTS,assert=(condition,message)=>{if(!condition)throw new Error(message);};
THREE.MeshStandardMaterial.prototype.clone=function(){return new this.constructor(this);};
const cloneMaterial=value=>Array.isArray(value)?value.map(material=>material.clone()):value.clone();
(async()=>{
 const side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A),old=new R.InfantryUnit({id:'preview-old',side,seed:41001,state:'IDLE',detail:'high'}),scene=new THREE.Group();
 scene.add(old.root);const preview=Object.create(R.UnitPreview.prototype);
 Object.assign(preview,{scene,sides:{SIDE_A:side},unit:old,_generateToken:0,previewMaterials:cloneMaterial(old.model.mesh.material),gearMaterials:cloneMaterial(old.model.gearMesh.material),skeletonHelper:null,colliders:null,clock:{reset(){}},dirty:false});
 old.model.mesh.material=preview.previewMaterials;old.model.gearMesh.material=preview.gearMaterials;
 let settingsUpdates=0;preview.updateSettings=()=>{settingsUpdates++;};
 const first=preview.generateAsync('SIDE_A',41002,'WALK','NEUTRAL',1,{variation:1},{loadout:'RIFLEMAN'});
 const second=preview.generateAsync('SIDE_A',41003,'RUN','NEUTRAL',1,{variation:1},{loadout:'RIFLEMAN'});
 assert(preview.unit===old&&!old.model.disposed,'old preview was removed before an async replacement committed');
 let firstError=null;try{await first;}catch(error){firstError=error;}
 assert(firstError&&firstError.name==='AbortError','superseded preview build did not cancel');
 assert(preview.unit===old&&!old.model.disposed,'cancelled replacement disturbed the visible preview');
 const committed=await second;
 assert(preview.unit===committed&&committed.model.detail==='high','latest async preview did not commit');
 assert(old.model.disposed,'previous preview resources were not released after commit');
 assert(settingsUpdates===1,'preview helpers/settings were rebuilt for a cancelled generation');
 assert(scene.children.includes(committed.root)&&!scene.children.includes(old.root),'scene contains the wrong preview root after commit');
 preview.disposeUnit();
 console.log('PASS async unit preview keeps old model through generation, cancels superseded builds, and commits/releases atomically');
})().catch(error=>{console.error(error);process.exitCode=1;});
