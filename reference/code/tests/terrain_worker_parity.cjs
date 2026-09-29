'use strict';
const assert=require('assert'),fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.BufferGeometry.prototype.setDrawRange=function(start,count){this.drawRange={start,count};return this;};
for(const file of ['js/config.js','js/core/seededRandom.js','js/noise.js','js/terrain.js'])
  vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});
const workerSource=fs.readFileSync(path.join(root,'js/workers/terrainHeightWorker.js'),'utf8');
function runWorker(data){
  let result=null;
  const scope={postMessage(message){result=message;}};
  vm.runInNewContext(workerSource,{self:scope,Math,Float32Array,Uint8Array});
  scope.onmessage({data});
  return result;
}
function verify(seed,mapSize,segments,battlefield){
  const terrain=new RTS.TerrainSystem({add(){},remove(){}},{mapSize,segments,battlefield}),height=terrain.makeHeightFunction(seed);
  const permutation=new RTS.SeededPerlin2D(seed).perm.slice();
  const palette={};for(const [name,hex] of [['grass',0x708454],['dry',0x8e9261],['rock',0x827f72]])palette[name]=new THREE.Color(hex).convertSRGBToLinear();
  const actual=runWorker({segments,mapSize,halfMap:RTS.Config.HALF_MAP,battlefield,permutation:permutation.buffer,palette});
  const workerHeights=new Float32Array(actual.heights);
  const step=mapSize/segments,half=mapSize/2,row=segments+1;
  assert.strictEqual(actual.heights.byteLength/4,row*row);
  for(let z=0;z<=segments;z++)for(let x=0;x<=segments;x++){
    const expected=Math.fround(height(x*step-half,z*step-half)),index=z*row+x;
    assert.strictEqual(workerHeights[index],expected,`worker height mismatch seed=${seed} battlefield=${battlefield} x=${x} z=${z}`);
  }
  terrain.dispose();
}
verify(0,200,40,true);
verify(8291,200,40,true);
verify(14523,RTS.Config.MAP_SIZE,40,false);
console.log('PASS terrain worker height fields are byte-exact to synchronous Perlin sampling');
