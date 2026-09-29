// Numeric differential test for the conservative clearance broad phase.
// Uses the repository's Three-like shim; this is not a WebGL test.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
global.window=global;
global.THREE=require('./numeric_math.cjs');
THREE.Box3.prototype.copy=function(box){this.min.copy(box.min);this.max.copy(box.max);return this;};
THREE.Box3.prototype.applyMatrix4=function(matrix){
  const lo=this.min.clone(),hi=this.max.clone();this.makeEmpty();
  for(let mask=0;mask<8;mask++)this.expandByPoint(new THREE.Vector3(
    mask&1?hi.x:lo.x,mask&2?hi.y:lo.y,mask&4?hi.z:lo.z).applyMatrix4(matrix));
  return this;
};
global.RTS={Math:{clamp:(v,a,b)=>Math.max(a,Math.min(b,v))},FlatSurface:{getHeightAt:()=>0}};
global.RTS.TerrainSystem=function TerrainSystem(){};
const R=RTS;
const root=path.resolve(__dirname,'..');
vmRun(fs.readFileSync(path.join(root,'js/animation/infantryAnimator.js'),'utf8'));
function vmRun(code){require('vm').runInThisContext(code,{filename:'js/animation/infantryAnimator.js'});}

function makeMesh(positions,skinIndices,skinWeights,bones,gear=false,morphs=[]){
  const identity=()=>new THREE.Matrix4();
  const geometry={attributes:{
    position:{array:new Float32Array(positions)},
    skinIndex:{array:new Uint16Array(skinIndices)},
    skinWeight:{array:new Float32Array(skinWeights)}
  },morphAttributes:{position:morphs.map(delta=>({array:new Float32Array(delta)}))},morphTargetsRelative:true};
  const influences=morphs.map(()=>0);
  const mesh={geometry,matrixWorld:identity(),bindMatrix:identity(),bindMatrixInverse:identity(),morphTargetInfluences:influences,skeleton:{boneInverses:bones.map(()=>identity())}};
  const base=new THREE.Vector3(),point=new THREE.Vector3(),matrix=new THREE.Matrix4();
  function skin(index,out){
    const g=geometry,offset=index*4;
    base.fromArray(g.attributes.position.array,index*3);
    for(let m=0;m<g.morphAttributes.position.length;m++){
      const w=mesh.morphTargetInfluences[m]||0;if(w)base.addScaledVector(point.fromArray(g.morphAttributes.position[m].array,index*3),w);
    }
    base.applyMatrix4(mesh.bindMatrix);out.set(0,0,0);
    for(let k=0;k<4;k++){
      const weight=g.attributes.skinWeight.array[offset+k];if(weight<=0)continue;
      const bone=g.attributes.skinIndex.array[offset+k];
      matrix.multiplyMatrices(bones[bone].matrixWorld,mesh.skeleton.boneInverses[bone]);
      point.copy(base).applyMatrix4(matrix);out.addScaledVector(point,weight);
    }
    return out.applyMatrix4(mesh.bindMatrixInverse);
  }
  return {mesh,skin,gear};
}

function surface(slopeX,slopeZ,base,irregular=false){
  const segments=24,mapSize=24,step=mapSize/segments,heights=new Float32Array((segments+1)**2);
  for(let z=0;z<=segments;z++)for(let x=0;x<=segments;x++){
    const wx=x*step-mapSize/2,wz=z*step-mapSize/2;
    const ripple=irregular?Math.sin(x*1.71+z*.37)*.11+Math.cos(z*1.23-x*.29)*.07:0;
    heights[z*(segments+1)+x]=base+slopeX*wx+slopeZ*wz+ripple;
  }
  const sample=(x,z)=>{
    const fx=Math.max(0,Math.min(segments,(x+mapSize/2)/step)),fz=Math.max(0,Math.min(segments,(z+mapSize/2)/step));
    const ix=Math.min(segments-1,Math.floor(fx)),iz=Math.min(segments-1,Math.floor(fz)),u=fx-ix,v=fz-iz,k=iz*(segments+1)+ix;
    const a=heights[k],d=heights[k+1],b=heights[k+segments+1],c=heights[k+segments+2];
    return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);
  };
  return Object.assign(Object.create(R.TerrainSystem.prototype),{segments,mapSize,heights,queries:0,
    getHeightAt(x,z){this.queries++;return sample(x,z);}});
}

const bones=[0,1,2,3].map(()=>({matrixWorld:new THREE.Matrix4()}));
bones[0].matrixWorld.makeTranslation(0,.12,0);
bones[1].matrixWorld.makeTranslation(0,-.18,0);
bones[2].matrixWorld.makeTranslation(.03,0,-.02);
bones[3].matrixWorld.makeTranslation(0,.04,0);
const positions=[0,1.8,0, -.10,.9,.05, .15,.04,.11, -.08,.3,-.06, .03,.2,.04];
const skinIndices=[0,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0, 1,2,0,0];
const skinWeights=[1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0, .65,.35,0,0];
const morphA=Array(positions.length).fill(0);morphA[1]=-.38;morphA[7]=.16;
const morphB=Array(positions.length).fill(0);morphB[2]=.12;morphB[6]=-.08;
const body=makeMesh(positions,skinIndices,skinWeights,bones,false,[morphA,morphB]);
const gear=makeMesh(positions,skinIndices,skinWeights,bones,true,[morphA,morphB]);
const model={mesh:body.mesh,gearMesh:gear.mesh,gear:{contactIndices:[0,1,2,3,4],headContactIndices:[0,1]},
  skinSampleRevision:0,prepareSkinPalette(){},skinVertex:body.skin,skinGearVertex:gear.skin};
const animator=Object.create(R.InfantryAnimator.prototype);
animator.model=model;animator.rig={bones};animator._w=new THREE.Vector3();
animator._clearanceMatrix=new THREE.Matrix4();animator._clearanceBox=new THREE.Box3();
animator._clearancePoint=new THREE.Vector3();
const indices=[0,1,2,3,4];
const terrains=[surface(0,0,.0),surface(.06,-.025,.2),surface(2.4,-1.8,-.7),surface(.12,-.09,.1,true)];
for(let terrainIndex=0;terrainIndex<terrains.length;terrainIndex++){
  const terrain=terrains[terrainIndex];
  for(const yOffset of [0,-.055,.42])for(const morphWeights of [[0,0],[1,0],[-.8,.6],[.35,-1]]){
    bones[2].matrixWorld.makeTranslation(.03,yOffset,-.02);
    body.mesh.morphTargetInfluences.splice(0,2,...morphWeights);gear.mesh.morphTargetInfluences.splice(0,2,...morphWeights);
    model.skinSampleRevision++;
    const exact=animator.minimumClearance(indices,terrain,false,false,false);
    const exactQueries=terrain.queries;terrain.queries=0;
    const broad=animator.minimumClearance(indices,terrain,false,true,false);
    assert.ok(Math.abs(exact-broad)<1e-5,`body mismatch exact=${exact} broad=${broad}`);
    if(terrainIndex===0&&yOffset===0&&morphWeights[0]===0&&morphWeights[1]===0)assert.ok(terrain.queries<exactQueries,`broadphase did not prune body samples (${terrain.queries}/${exactQueries})`);
    terrain.queries=0;
    const gearExact=animator.minimumClearance(indices,terrain,false,false,true);
    terrain.queries=0;
    const gearBroad=animator.minimumGearClearance(terrain,false,false,true);
    assert.ok(Math.abs(gearExact-gearBroad)<1e-5,`gear mismatch exact=${gearExact} broad=${gearBroad}`);
  }
}
// Shared immutable geometry/support indices do not make transformed group
// bounds shareable: each model owns an independent pose revision sequence.
const otherBones=[0,1,2,3].map(()=>({matrixWorld:new THREE.Matrix4()}));
otherBones[0].matrixWorld.makeTranslation(0,20,0);otherBones[1].matrixWorld.makeTranslation(0,-20,0);
otherBones[2].matrixWorld.makeTranslation(0,20,0);otherBones[3].matrixWorld.makeTranslation(0,20,0);
const otherBody=makeMesh(positions,skinIndices,skinWeights,otherBones,false,[morphA,morphB]);
otherBody.mesh.geometry=body.mesh.geometry;
otherBody.mesh.morphTargetInfluences.splice(0,2,...body.mesh.morphTargetInfluences);
const otherModel={mesh:otherBody.mesh,gearMesh:null,gear:null,skinSampleRevision:model.skinSampleRevision,prepareSkinPalette(){},skinSupportVertex:otherBody.skin};
const otherAnimator=Object.create(R.InfantryAnimator.prototype);
otherAnimator.model=otherModel;otherAnimator.rig={bones:otherBones};otherAnimator._w=new THREE.Vector3();
otherAnimator._clearanceMatrix=new THREE.Matrix4();otherAnimator._clearanceBox=new THREE.Box3();otherAnimator._clearancePoint=new THREE.Vector3();
const cacheSafetySurface=surface(0,0,0);
let otherPoseBounds=0;const countPoseBounds=THREE.Box3.prototype.applyMatrix4;
THREE.Box3.prototype.applyMatrix4=function(matrix){otherPoseBounds++;return countPoseBounds.call(this,matrix);};
const otherExact=otherAnimator.minimumClearance(indices,cacheSafetySurface,false,false,false);
const otherBroad=otherAnimator.minimumClearance(indices,cacheSafetySurface,false,true,false);
THREE.Box3.prototype.applyMatrix4=countPoseBounds;
assert.ok(otherPoseBounds>0,'shared geometry reused another model\'s transformed group bounds');
assert.ok(Math.abs(otherExact-otherBroad)<1e-5,`shared-geometry pose cache mismatch exact=${otherExact} broad=${otherBroad}`);
// Repeated broadphase queries at one exact pose reuse transformed group boxes;
// an explicit skin revision invalidates them before the next pose.
let transformedBoxes=0;const originalApply=THREE.Box3.prototype.applyMatrix4;
THREE.Box3.prototype.applyMatrix4=function(matrix){transformedBoxes++;return originalApply.call(this,matrix);};
const cacheTerrain=terrains[0];
animator.minimumClearance(indices,cacheTerrain,false,true,false);const firstTransforms=transformedBoxes;
animator.minimumClearance(indices,cacheTerrain,false,true,false);
assert.equal(transformedBoxes,firstTransforms,'unchanged skin revision recomputed transformed group bounds');
model.skinSampleRevision++;
animator.minimumClearance(indices,cacheTerrain,false,true,false);
assert.equal(transformedBoxes,firstTransforms,'revision change without bone/morph changes recomputed stable group bounds');
bones[2].matrixWorld.makeTranslation(.19,.42,-.02);model.skinSampleRevision++;
animator.minimumClearance(indices,cacheTerrain,false,true,false);
assert.equal(transformedBoxes-firstTransforms,1,'single-bone pose change should transform only its clearance group bounds');
const beforeMorphTransforms=transformedBoxes;body.mesh.morphTargetInfluences[0]+=.1;model.skinSampleRevision++;
animator.minimumClearance(indices,cacheTerrain,false,true,false);
assert.equal(transformedBoxes-beforeMorphTransforms,2,'morph change should transform only groups containing morphed support vertices');
// Clearance height sampling reuses the terrain's exact grid-cell cache.
const cachedTerrain={heights:new Float32Array([0,0,0,0]),segments:1,mapSize:1,_step:1,_half:.5,
  getHeightAtCached(x,z,cache){
    const N=this.segments,step=this._step,half=this._half,fx=Math.max(0,Math.min(N,(x+half)/step)),fz=Math.max(0,Math.min(N,(z+half)/step));
    const ix=Math.min(N-1,Math.floor(fx)),iz=Math.min(N-1,Math.floor(fz));
    if(cache&&(!((cache.terrain===this)&&(cache.heights===this.heights))||cache.ix!==ix||cache.iz!==iz)){
      const k=iz*(N+1)+ix;cache.terrain=this;cache.heights=this.heights;cache.ix=ix;cache.iz=iz;
      cache.a=this.heights[k];cache.d=this.heights[k+1];cache.b=this.heights[k+N+1];cache.c=this.heights[k+N+2];
    }
    const u=fx-ix,v=fz-iz,a=cache.a,d=cache.d,b=cache.b,c=cache.c;
    return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);
  },getHeightAt(x,z){const N=this.segments,step=this._step,half=this._half,fx=Math.max(0,Math.min(N,(x+half)/step)),fz=Math.max(0,Math.min(N,(z+half)/step)),ix=Math.min(N-1,Math.floor(fx)),iz=Math.min(N-1,Math.floor(fz)),u=fx-ix,v=fz-iz,k=iz*(N+1)+ix,a=this.heights[k],d=this.heights[k+1],b=this.heights[k+N+1],c=this.heights[k+N+2];return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);}};
let uncachedQueries=0;cachedTerrain.getHeightAt=function(x,z){uncachedQueries++;return 0;};
for(const [x,z] of [[-.49,-.49],[0,0],[.49,.49],[-.2,.37]])assert.ok(Math.abs(cachedTerrain.getHeightAtCached(x,z,{terrain:null,heights:null,ix:-1,iz:-1})-cachedTerrain.getHeightAt(x,z))<1e-12,'fixture cache interpolation differs from the scalar reference');
uncachedQueries=0;
animator._clearanceHeightCache={terrain:null,heights:null,ix:-1,iz:-1,a:0,b:0,c:0,d:0};
animator.minimumClearance(indices,cachedTerrain,false,false,false);
assert.equal(uncachedQueries,0,'clearance bypassed the grid cell height cache');
const oldHeights=cachedTerrain.heights;cachedTerrain.heights=new Float32Array([.25,.25,.25,.25]);
animator.minimumClearance(indices,cachedTerrain,false,false,false);
assert.notEqual(animator._clearanceHeightCache.heights,oldHeights,'clearance height cache survived a backing terrain replacement');
console.log('Clearance broadphase matches exact scans on flat, sloped, steep, and near-contact samples.');
