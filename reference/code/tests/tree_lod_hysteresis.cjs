'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
global.THREE=require('./numeric_math.cjs');
global.window={RTS:{}};
const R=window.RTS;
vm.runInThisContext(fs.readFileSync(require.resolve('../js/world/lodHysteresis.js'),'utf8'),{filename:'lodHysteresis.js'});
const objects=[{visible:false},{visible:false},{visible:false}],lod={
  position:new THREE.Vector3(4,2,-3),
  levels:[{distance:0,object:objects[0]},{distance:65,object:objects[1]},{distance:130,object:objects[2]}],
  getWorldPosition(target){return target.copy(this.position);}
};
R.LodHysteresis.install(lod);
const camera={position:new THREE.Vector3(),zoom:1,getWorldPosition(target){return target.copy(this.position);}};
const at=distance=>{camera.position.copy(lod.position).x+=distance;lod.update(camera);assert.equal(objects.filter(object=>object.visible).length,1,'exactly one tree LOD must be visible');return lod._currentLevel;};
assert.equal(at(65),0,'keep detailed mesh at nominal threshold while moving outward');
assert.equal(at(71),1,'switch to middle LOD beyond outer threshold');
assert.equal(at(65),1,'keep middle LOD inside hysteresis band');
assert.equal(at(59),0,'switch back only beyond inner threshold');
assert.equal(at(141),2,'switch directly to far LOD when camera jumps beyond it');
assert.equal(at(125),2,'keep far LOD inside far hysteresis band');
assert.equal(at(119),1,'return to middle LOD beyond inner far threshold');
assert.throws(()=>R.LodHysteresis.install(lod,1),RangeError);
console.log('PASS tree LOD hysteresis: outward/inward thresholds, tier jumps, visibility and validation');
