'use strict';
const assert=require('assert');
const fs=require('fs');
const vm=require('vm');

let terrainBuilds=0,infantryBuilds=0,battlefieldBuilds=0,appearanceCacheClears=0,detailBuilds=0;
class Vec3 {
  constructor(x=0,y=0,z=0){this.set(x,y,z);}
  set(x,y,z){this.x=x;this.y=y;this.z=z;return this;}
  copy(v){return this.set(v.x,v.y,v.z);}
  clone(){return new Vec3(this.x,this.y,this.z);}
  addScaledVector(v,s){this.x+=v.x*s;this.y+=v.y*s;this.z+=v.z*s;return this;}
  normalize(){const n=Math.hypot(this.x,this.y,this.z)||1;return this.set(this.x/n,this.y/n,this.z/n);}
  distanceTo(v){return Math.sqrt(this.distanceToSquared(v));}
  distanceToSquared(v){const x=this.x-v.x,y=this.y-v.y,z=this.z-v.z;return x*x+y*y+z*z;}
}
class Scene {constructor(){this.children=[];}add(...items){for(const item of items)if(!this.children.includes(item))this.children.push(item);}remove(item){const i=this.children.indexOf(item);if(i>=0)this.children.splice(i,1);}}
class Camera {constructor(){this.position=new Vec3();this.quaternion={x:0,y:0,z:0,w:1};this.up=new Vec3(0,1,0);this.projectionMatrix={elements:new Float64Array(16)};this.matrixWorldInverse={};this.matrixWorldUpdates=0;}updateProjectionMatrix(){}updateMatrixWorld(){this.matrixWorldUpdates++;}}
class Renderer {constructor(){this.domElement={};this.shadowMap={};this.info={memory:{geometries:2,textures:1}};}setPixelRatio(){}setSize(){}render(){}dispose(){}}
class Light {constructor(){this.position=new Vec3();this.target={position:new Vec3()};this.shadow={mapSize:{set(){}},camera:{updateProjectionMatrix(){}}};}}
class Controls {constructor(camera){this.camera=camera;this.target=new Vec3();}update(){}dispose(){}}
class Matrix4 {multiplyMatrices(){return this;}}
class Frustum {constructor(){this.visible=true;this.projectionUpdates=0;}setFromProjectionMatrix(){this.projectionUpdates++;return this;}intersectsSphere(){return this.visible;}}
class Sphere {constructor(center,radius){this.center=center;this.radius=radius;}}

const elements=new Map();
function element(id){if(!elements.has(id))elements.set(id,{id,disabled:true,checked:false,open:false,textContent:id==='cacheStats'?'Cache geometrii: 0 / 64 MiB':'',appendChild(){},addEventListener(){},removeEventListener(){}});return elements.get(id);}
let raf=[],loggedErrors=[];
const window={devicePixelRatio:1,addEventListener(){},removeEventListener(){}};
const document={hidden:false,getElementById:element,addEventListener(){},removeEventListener(){}};
const R={
  Config:{CAMERA:{FOV:50,NEAR:.1,FAR:500,MIN_DISTANCE:1,MAX_DISTANCE:500},SIDES:{A:{},B:{}}},
  RenderQuality:{get(){return 'balanced';},ratio(){return 1;},onChange(){return ()=>{};}},
  TerrainSystem:class {constructor(){throw Error('startup constructed terrain');}build(){terrainBuilds++;}},
  InfantryUnit:class {constructor(){infantryBuilds++;throw Error('startup constructed infantry');}},
  Side:class {constructor(id){this.id=id;}addUnit(){}},
  FixedClock:class {reset(){}advance(){return 0;}},
  DebugPanel:class {constructor(){element('openUnits').disabled=false;this.isOpen=false;}dispose(){}},
  AppearanceCache:{stats(){return{activeBytes:2*1048576,peakActiveBytes:4*1048576,inactiveBytes:0,limit:0,entries:0,hits:0,keyStats:[]};},clear(){appearanceCacheClears++;}},
  PlantGenerator:{memoryStats(){return{retainedGeometryBytes:1048576,peakRetainedGeometryBytes:2097152,worldEntries:3,timberEntries:3};}},
  InfantryMaterials:{dispose(){}},InfantryGear:{dispose(){}}
};
window.RTS=R;
R.Battlefield=class {
  constructor(seed){battlefieldBuilds++;this.seed=seed;
    const sharedBuffer=new ArrayBuffer(32),morphBuffer=new ArrayBuffer(12),indexBuffer=new ArrayBuffer(4),geometry={
      attributes:{position:{array:new Float32Array(sharedBuffer,0,6)},color:{array:new Uint8Array(sharedBuffer,16,4)}},
      morphAttributes:{position:[{array:new Float32Array(morphBuffer)}]},index:{array:new Uint16Array(indexBuffer)}
    };
     const instanceMatrix={array:new Float32Array(new ArrayBuffer(64))},instanceColor={array:new Uint8Array(new ArrayBuffer(16))};
     this.root={children:[{geometry,children:[]},{geometry,children:[]},{isInstancedMesh:true,instanceMatrix,instanceColor,children:[]}]};this.units=Array.from({length:50},()=>({side:{addUnit(){},removeUnit(){}},position:new Vec3(),animator:{H:1},model:{detail:'far'},setVisualCadence(animation,face){this.cadence=[animation,face];},setDetailAsync(){detailBuilds++;return Promise.resolve(false);},dispose(){},step(){},render(){}}));
     this.terrain={getHeightAt(){if(seed===99)throw Error('simulated commit setup failure');return 0;},setSmallGridVisible(){},setBigGridVisible(){},dispose(){}};}
  async build(progress){progress(0,'terrain');progress(50,'army');}
  dispose(){}
};
const THREE={
  Scene,Color:class {constructor(value){this.value=value;}},FogExp2:class {},
  PerspectiveCamera:Camera,WebGLRenderer:Renderer,OrbitControls:Controls,
  HemisphereLight:class {},DirectionalLight:Light,Vector3:Vec3,Matrix4,Frustum,Sphere,
  sRGBEncoding:1,ACESFilmicToneMapping:2,PCFSoftShadowMap:3
};
const context={window,document,innerWidth:1280,innerHeight:720,THREE,RTS:R,Object,
  requestAnimationFrame:cb=>{raf.push(cb);return raf.length;},cancelAnimationFrame(){},
  AbortController,performance:{now(){return 0;},memory:{usedJSHeapSize:8*1048576}},crypto:{getRandomValues(a){a[0]=123;return a;}},console:{error(...args){loggedErrors.push(args);}}};
vm.runInNewContext(fs.readFileSync(require.resolve('../js/game.js'),'utf8'),context,{filename:'game.js'});

(async()=>{
  const game=new R.Game();
  assert.strictEqual(game.terrain,null,'terrain should remain unbuilt before user action');
  assert.strictEqual(game.units.length,0,'demo infantry should not be constructed at startup');
  assert.strictEqual(terrainBuilds,0);
  assert.strictEqual(infantryBuilds,0);
  assert.strictEqual(battlefieldBuilds,0);
  assert.strictEqual(element('openUnits').disabled,false,'infantry editor remains available');
  assert.strictEqual(element('startNew').disabled,false,'START NEW remains available');
  assert.match(element('worldSize').textContent,/nieutworzony/);

  const starting=game.startNew(42);
  assert.strictEqual(battlefieldBuilds,1,'explicit START NEW builds the scenario');
  assert.strictEqual(appearanceCacheClears,1,'START NEW clears inactive retained appearances before building a candidate world');
  raf[1](0); // resolve the one-frame yield before procedural generation
  await starting;
  assert.strictEqual(game.units.length,50);
  assert.strictEqual(game.terrain.getHeightAt(0,0),0);
  assert.match(element('worldSize').textContent,/200 × 200 m/);
  assert.strictEqual(element('startNew').disabled,false);
  const previous=game.battlefield,previousRoot=previous.root;element('performancePanel').open=true;const failedStart=game.startNew(99);
  raf[2](0);await failedStart;
  assert.strictEqual(game._memoryHighWater.geometryBufferBytes,256,'build-time geometry peak includes shared geometry plus instance matrix/color buffers for old and candidate roots');
  element('performancePanel').open=false;
  assert.strictEqual(game.battlefield,previous,'failed candidate setup preserves the running battlefield');
  assert.strictEqual(game.scene.children.includes(previousRoot),true,'failed candidate setup keeps the old world attached');
  assert.strictEqual(game.terrain,previous.terrain,'failed candidate setup restores the old terrain');
  assert.strictEqual(loggedErrors.length,1,'failed candidate setup reports the original error');
  assert.strictEqual(element('startNew').disabled,false,'a failed replacement permits another start attempt');
  game.lastLOD=499;game.loop(600);
  assert.equal(game.camera.matrixWorldUpdates,1,'initial visibility sample builds the LOD camera matrix');
  assert.equal(game._lodFrustum.projectionUpdates,1,'initial visibility sample builds the LOD frustum');
  assert.equal(element('cacheStats').textContent,'Cache geometrii: 0 / 64 MiB','collapsed performance panel should not run memory telemetry');
  element('performancePanel').open=true;game.lastCacheStats=0;let telemetryValuesCalls=0;const originalValues=Object.values;
  Object.values=function(...args){telemetryValuesCalls++;return originalValues.apply(this,args);};
  try{game.loop(601);}finally{Object.values=originalValues;}
  assert.strictEqual(telemetryValuesCalls,0,'scene buffer telemetry should enumerate geometry attributes without allocating Object.values arrays');
  assert.match(element('cacheStats').textContent,/JS heap Chromium: 8\.0 \/ 8\.0 MiB/,'debug panel reports the sampled JS heap and observed high-water');
  assert.match(element('cacheStats').textContent,/2\.0 aktywne \/ 4\.0 MiB szczyt appearance/,'debug panel shows the exact appearance-buffer high-water independently of sampled heap');
  assert.match(element('cacheStats').textContent,/2 \/ 2 geometrii, 1 \/ 1 tekstur/,'debug panel reports current and observed renderer resource counts');
  assert.match(element('cacheStats').textContent,/128 B \/ 256 B teraz \/ szczyt/,'debug panel preserves old-plus-candidate geometry and instance buffers after rollback');
  assert.match(element('cacheStats').textContent,/geometria roślin w cache: 1\.0 MiB \/ 2\.0 MiB teraz \/ szczyt \(unikalne bufory; podzbiór aktywnej sceny\) · 3 warianty LOD, 3 pnie/,'debug panel labels exact cached plant geometry as a scene-buffer subset');
  let lodDesiredReads=0;
  for(const unit of game.units){let desired=unit._lodDesiredDetail;Object.defineProperty(unit,'_lodDesiredDetail',{configurable:true,get(){lodDesiredReads++;return desired;},set(value){desired=value;}});}
  game.lastLOD=600;game.loop(601);
  assert.strictEqual(lodDesiredReads,0,'LOD queue selection does not rescan desired details between visibility ticks');
  game.lastLOD=500;game.loop(700);
  assert.equal(game.camera.matrixWorldUpdates,1,'unchanged camera reused its LOD view matrix');
  assert.equal(game._lodFrustum.projectionUpdates,1,'unchanged camera reused its LOD frustum');
  game.camera.position.x+=.01;game.lastLOD=699;game.loop(800);
  assert.equal(game.camera.matrixWorldUpdates,2,'camera movement invalidated its LOD view matrix');
  assert.equal(game._lodFrustum.projectionUpdates,2,'camera movement invalidated its LOD frustum');
  game.camera.position.x-=.01;game.camera.projectionMatrix.elements[0]+=1;game.lastLOD=799;game.loop(900);
  assert.equal(game.camera.matrixWorldUpdates,3,'projection resize invalidated its LOD view matrix');
  assert.equal(game._lodFrustum.projectionUpdates,3,'projection resize invalidated its LOD frustum');
   game.units[0].model.detail='world';game.lastLOD=0;game._lodFrustum.visible=false;
   let cameraDistanceCalls=0;
   game.camera.position.distanceTo=function(v){cameraDistanceCalls++;return Math.sqrt(this.distanceToSquared(v));};
   game.loop(700);
   assert.strictEqual(cameraDistanceCalls,1,'visibility scheduling adds no square-root distance queries beyond the shadow-region query');
  assert(lodDesiredReads>0,'LOD queue selection runs with the visibility refresh');
  assert.deepEqual(game.units[0].cadence,[.20,.20],'offscreen units should use the 5 Hz visual cadence');
  assert.equal(game.units[0]._lodDesiredDetail,'world','offscreen units should defer geometry LOD changes');
  assert.equal(detailBuilds,0,'offscreen LOD work should not be queued');
  game.lastLOD=0;game._lodFrustum.visible=true;game.loop(800);
  await Promise.resolve();await Promise.resolve();
  assert.deepEqual(game.units[0].cadence,[1/15,.10],'visible far units retain the existing 15 Hz animation and 10 Hz face cadence');
   assert.equal(game.units[0]._lodDesiredDetail,'far','visible units request the distance-selected far geometry');
   assert.equal(detailBuilds,1,'visible unit resumes its deferred LOD build');
   for(let i=1;i<game.units.length;i++)game.units[i].model.detail='high';
   const thresholdUnit=game.units[0];thresholdUnit.model.detail='world';
   let thresholdNow=900;
   function checkThreshold(distance,detail,animation,face){
     thresholdUnit.position.set(game.camera.position.x+distance,game.camera.position.y,game.camera.position.z);thresholdUnit.model.detail=detail;game.lastLOD=thresholdNow-101;game.loop(thresholdNow++);
     assert.deepEqual(thresholdUnit.cadence,[animation,face],'visual cadence boundary '+distance+'m');
     return thresholdUnit._lodDesiredDetail;
   }
   assert.equal(checkThreshold(10,'world',0,1/30),'world','10 m is inside the medium face cadence, outside the near geometry switch');
   assert.equal(checkThreshold(15,'world',1/30,1/30),'world','15 m uses medium animation cadence');
   assert.equal(checkThreshold(40,'world',1/15,.10),'world','40 m uses the far animation cadence');
   assert.equal(checkThreshold(55,'far',1/15,.10),'far','55 m remains inside the far-geometry hysteresis band');
   assert.equal(checkThreshold(65,'world',1/15,.10),'world','65 m does not cross the outward geometry threshold');
   assert.equal(checkThreshold(65.01,'world',1/15,.10),'far','distance above 65 m crosses the outward geometry threshold');
   console.log('PASS lazy startup: no terrain/demo units at boot; editor available; START NEW builds 50 units; memory telemetry reports samples');
})().catch(error=>{console.error(error);process.exitCode=1;});
