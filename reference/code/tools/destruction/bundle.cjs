const fs=require('node:fs'),path=require('node:path');
const dir=path.resolve(__dirname,'../../js/vendor');
fs.mkdirSync(dir,{recursive:true});
require('esbuild').buildSync({stdin:{contents:'import R from "@dimforge/rapier3d-compat"; globalThis.RapierCompat=R;',resolveDir:__dirname},bundle:true,format:'iife',minify:true,outfile:path.join(dir,'rapier-0.19.3.js'),banner:{js:'/* Rapier 3D compat 0.19.3; Apache-2.0. Rebuild: npm ci && npm run bundle in tools/destruction. */'}});
// The npm package omits LICENSE; keep the upstream v0.19.3 Apache license locally.
if(!fs.existsSync(path.join(dir,'RAPIER-LICENSE')))throw Error('Missing js/vendor/RAPIER-LICENSE from dimforge/rapier.js v0.19.3');
const hull=fs.readFileSync(path.join(__dirname,'node_modules/three/examples/jsm/math/ConvexHull.js'),'utf8').replace(/import\s*\{([\s\S]*?)\}\s*from 'three';/,'const {$1}=globalThis.THREE;').replace(/export\s*\{[\s\S]*?\};/,'');
const expose='globalThis.DestructionConvexFaces=points=>new ConvexHull().setFromPoints(points).faces.map(f=>{const out=[];let e=f.edge;do{out.push(e.head().point.toArray());e=e.next;}while(e!==f.edge);return out;});';
require('esbuild').buildSync({stdin:{contents:hull+'\n'+expose,resolveDir:__dirname},bundle:true,format:'iife',minify:true,outfile:path.join(dir,'convex-hull-r128.js'),banner:{js:'/* Three.js r128 ConvexHull; MIT. Uses the host THREE instance. See THREE-LICENSE. */'}});
fs.copyFileSync(path.join(__dirname,'node_modules/three/LICENSE'),path.join(dir,'THREE-LICENSE'));
const crypto=require('node:crypto');
const files=['rapier-0.19.3.js','convex-hull-r128.js','RAPIER-LICENSE','THREE-LICENSE'];
fs.writeFileSync(path.join(dir,'manifest.json'),JSON.stringify({rapier:'0.19.3',three:'0.128.0',esbuild:'0.25.5',sha256:Object.fromEntries(files.map(file=>[file,crypto.createHash('sha256').update(fs.readFileSync(path.join(dir,file))).digest('hex')]))},null,2)+'\n');
