'use strict';
const path=require('node:path'),fs=require('node:fs'),crypto=require('node:crypto'),esbuild=require('esbuild');
const root=path.resolve(__dirname,'../..'),outfile=path.join(root,'js/vendor/buildings-6.0.0.js');
esbuild.buildSync({entryPoints:[path.join(__dirname,'vendor-entry.js')],bundle:true,format:'iife',platform:'browser',target:'es2020',outfile,legalComments:'eof',banner:{js:'var self=globalThis;'},loader:{'.wasm':'binary'}});
const data=fs.readFileSync(outfile),sha=crypto.createHash('sha256').update(data).digest('hex');
fs.writeFileSync(path.join(__dirname,'manifest.sha256'),sha+'  js/vendor/buildings-6.0.0.js\n');
console.log('built',path.relative(root,outfile),sha);
