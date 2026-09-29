'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../js/world/worldDestructionHost.js'),'utf8');assert(source.includes('budgetMs'));assert(source.includes('new R.MaterialModel'));assert(source.includes('queueDamage'));console.log('PASS BuildingLazyDestruction contract');
