const fs=require('fs'),vm=require('vm'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
THREE.Color.prototype.clone=function(){return new THREE.Color().copy(this);};
THREE.Color.prototype.multiplyScalar=function(s){this.r*=s;this.g*=s;this.b*=s;return this;};
THREE.BufferGeometry.prototype.computeVertexNormals=function(){this.setAttribute('normal',new THREE.Float32BufferAttribute(new Float32Array(this.attributes.position.count*3),3));};
for(const file of ['js/core/seededRandom.js','js/environment/plantCatalog.js','js/environment/plantGenerator.js'])
  vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});

const model=RTS.PlantGenerator.create(RTS.PlantGenome.create('elder',0,{fruiting:1}),{season:'summer',age:.83,detail:'high'});
const hash=crypto.createHash('sha256');
for(const mesh of model.root.children){
  hash.update(mesh.name);
  const color=mesh.geometry.attributes.color,normal=mesh.geometry.attributes.normal,position=mesh.geometry.attributes.position;
  if(!(position.array instanceof Float32Array)||color.array.constructor!==Uint16Array||normal.array.constructor!==Int16Array||!color.normalized||!normal.normalized)
    throw new Error('Plant geometry must keep Float32 positions and normalized 16-bit color/normal attributes');
  for(let i=0;i<normal.count;i++){
    const o=i*3,x=normal.array[o]/32767,y=normal.array[o+1]/32767,z=normal.array[o+2]/32767,length=Math.sqrt(x*x+y*y+z*z);
    if(length!==0&&Math.abs(length-1)>.0001)throw new Error('Packed plant normal exceeded the quantization tolerance: '+length);
  }
  for(const name of ['position','color','normal']){
    const array=mesh.geometry.attributes[name].array;
    hash.update(Buffer.from(array.buffer,array.byteOffset,array.byteLength));
  }
}
model.dispose();
const digest=hash.digest('hex');
if(digest!=='9c3f4c486d24192845fa3304b6e6291d17a0fed914dcda2280dbe0cd60b36335')
  throw new Error('Packed plant buffers changed positions, colors, normals, or triangle ordering: '+digest);
console.log('PASS plant positions/triangle order stay golden with normalized 16-bit colors and normals');
