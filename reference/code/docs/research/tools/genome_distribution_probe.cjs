// Reproducible read-only algorithm audit; numerical geometry, NOT a visual or medical assessment.
const fs=require('fs'),path=require('path'),vm=require('vm'),crypto=require('crypto');
const root=path.resolve(__dirname,'../../..'),outDir=path.join(root,'tests/out');
fs.mkdirSync(outDir,{recursive:true});
global.window=global;global.THREE=require(path.join(root,'tests/numeric_math.cjs'));
const files=['js/config.js','js/core/seededRandom.js','js/units/infantryGenome.js','js/rendering/faceAnatomy.js','js/rendering/infantryRig.js'];
for(const file of files)vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});
const R=RTS, N=2000;
const stats=xs=>{const v=xs.slice().sort((a,b)=>a-b),n=v.length,mean=v.reduce((s,x)=>s+x,0)/n;const q=t=>v[Math.floor(t*(n-1))];return{min:v[0],p05:q(.05),median:q(.5),p95:q(.95),max:v[n-1],mean,sd:Math.sqrt(v.reduce((s,x)=>s+(x-mean)**2,0)/n)};};
const pearson=(xs,ys)=>{const a=stats(xs).mean,b=stats(ys).mean;let cov=0,vx=0,vy=0;xs.forEach((x,i)=>{cov+=(x-a)*(ys[i]-b);vx+=(x-a)**2;vy+=(ys[i]-b)**2;});return cov/Math.sqrt(vx*vy);};
const flatten=g=>Object.fromEntries(['body','face'].flatMap(group=>Object.entries(g[group]).map(([k,v])=>[group+'.'+k,v])));
function evaluate(seed,variation){
 const raw=R.InfantryGenome.create(seed),g=R.InfantryGenome.applyVariation(raw,variation),p=R.InfantryGenome.express(g),f=p.face,b=p.body,F=new R.FaceAnatomy(f,b),a=R.InfantryAnatomy.create(p.height,f,b);
 const flat=flatten(g),eye=F.eyes.L,cheek=F.section(.925+f.cheekboneY)[1],jaw=F.section(.900)[1],temple=F.section(.951)[1],neck=F.neck.radiusX;
 const repeatedPhenotype=R.InfantryGenome.express(R.InfantryGenome.applyVariation(R.InfantryGenome.create(seed),variation));
 const repeatedLayout=new R.FaceAnatomy(repeatedPhenotype.face,repeatedPhenotype.body);
 const chinRaw=.925+(.881-.925)*f.jawLengthScale+f.chinHeight*Math.exp(-(((.881-.887)/.015)**2))*.65;
 const faceGenes=Object.entries(g.face).filter(([k])=>!/(Color|Style|Brightness|resting|Asymmetry|blink|gaze|Expression|expression)/.test(k));
 const metrics={
  jawCheek:jaw/cheek,chinCheek:F.levels[4][1]/cheek,neckCheek:neck/cheek,neckJaw:neck/jaw,
  headWidthHeight:2*Math.max(...F.levels.slice(4).map(x=>x[1]))/(F.topY-F.chinY),headChinHeight:F.topY-F.chinY,
  cheekTemple:cheek/temple,eyeGapEyeWidth:(2*Math.abs(eye.x)-2*eye.w)/(2*eye.w),eyeWidthFaceWidth:eye.w/F.section(eye.y)[1],
  eyeHeightWidth:eye.h/eye.w,mouthWidthFaceWidth:F.mouthHalf/F.section(F.mouthY)[1],mouthNoseGap:F.noseBaseY-F.mouthY,
  noseLengthActual:f.eyeY+.005-F.noseBaseY,noseLengthRequested:.005+.031*f.noseLengthScale,
  mouthChinGap:F.mouthY-F.chinY,eyeMouthGap:eye.y-F.mouthY,eyeBrowGap:F.brows.L.inner.y-eye.y,
  noseWidthMouthWidth:.0068*f.noseWidthScale*f.noseTipWidthScale/F.mouthHalf,
  nostrilOuterMouthWidth:(.0065*f.noseWidthScale*f.nostrilWidthScale+.0035*f.nostrilWidthScale)/F.mouthHalf,
  handCuffWidth:.023*b.handScale/(.0262*b.armThicknessScale),
  shoulderHeadWidth:a.shoulderHalf/Math.max(...F.levels.slice(4).map(x=>x[1])),handForearmLength:.094*b.handScale/(.155*b.armLengthScale),
  neckSkullWidthJump:(F.levels[4][1]-F.levels[3][1])/(F.levels[4][0]-F.levels[3][0]),
  chinY:F.chinY,neckPivotY:a.points.neck.y/p.height,headPivotY:a.points.head.y/p.height,
  extremeFaceGenes:faceGenes.filter(([,v])=>v<.1||v>.9).length,clampedFaceGenes:faceGenes.filter(([,v])=>v===0||v===1).length,
  adjustedFeatures:F.adjustments.length,adjustedFeatureDeltaSum:F.adjustments.reduce((s,x)=>s+Math.abs(x.resolved-x.requested),0),
 };
 const chinClamp=Math.abs(chinRaw-F.chinY)>1e-8;
 return{seed,variation,metrics,adjustments:F.adjustments,chinClamp,chinClampDelta:F.chinY-chinRaw,flat,
  immutable:Object.isFrozen(g)&&Object.isFrozen(g.face)&&Object.isFrozen(g.body),
  repeatable:JSON.stringify(raw)===JSON.stringify(R.InfantryGenome.create(seed))&&JSON.stringify(p)===JSON.stringify(repeatedPhenotype)&&JSON.stringify(F.levels)===JSON.stringify(repeatedLayout.levels),
  rawWidths:{jaw,cheek,temple,neck,chin:F.levels[4][1]},
  identity:{hairStyle:f.hairStyle,skinColor:b.skinColor},
 };
}
const summary={date:'2026-09-24',generatorVersion:R.Config.GENERATOR_VERSION,environment:'Node + existing numeric_math.cjs Vector3 only. No Three.js/WebGL.',sample:'integer seeds 0..1999 at each variation',files:Object.fromEntries(files.map(f=>[f,crypto.createHash('sha256').update(fs.readFileSync(path.join(root,f))).digest('hex')])),runs:{}};
const all={};
for(const variation of [0,.5,1,1.25,1.75]){
 const rows=Array.from({length:N},(_,seed)=>evaluate(seed,variation));all[variation]=rows;
 const adjustments={};for(const r of rows)for(const a of r.adjustments){(adjustments[a.key]??=[]).push({...a,seed:r.seed,delta:Math.abs(a.resolved-a.requested)});}
 const metrics=Object.fromEntries(Object.keys(rows[0].metrics).map(k=>[k,stats(rows.map(r=>r.metrics[k]))]));
 const genes=Object.keys(rows[0].flat),geneClampCounts=Object.fromEntries(genes.map(k=>[k,rows.filter(r=>r.flat[k]===0||r.flat[k]===1).length]));
 summary.runs[variation]={N,repeatable:rows.every(r=>r.repeatable),immutable:rows.every(r=>r.immutable),anyAdjustment:rows.filter(r=>r.adjustments.length).length,
  chinClamp:{count:rows.filter(r=>r.chinClamp).length,delta:stats(rows.map(r=>r.chinClampDelta))},
  adjustments:Object.fromEntries(Object.entries(adjustments).map(([k,v])=>[k,{count:v.length,delta:stats(v.map(x=>x.delta)),largest:v.slice().sort((a,b)=>b.delta-a.delta).slice(0,3)}])),
  metrics,geneClampCounts,
  anyGeometryGeneEndpoint:rows.filter(r=>r.metrics.clampedFaceGenes).length,
  faceGeneCount:Object.keys(rows[0].flat).filter(k=>k.startsWith('face.')).length,
  extremes:Object.fromEntries(Object.keys(metrics).map(k=>[k,{min:rows.reduce((a,b)=>a.metrics[k]<b.metrics[k]?a:b).seed,max:rows.reduce((a,b)=>a.metrics[k]>b.metrics[k]?a:b).seed}]))
 };
}
const rows=all[1];const pairs=[['face.jawWidthGene','face.cheekboneWidthGene'],['face.jawWidthGene','body.neckThicknessGene'],['face.eyeWidthGene','face.eyeSpacingGene'],['face.noseLengthGene','face.mouthHeightGene'],['face.headWidthGene','body.headScaleGene'],['body.neckThicknessGene','body.shoulderBreadthGene'],['body.handScaleGene','body.armLengthGene']];
const faceOnlyKeys=Object.keys(rows[0].flat).filter(k=>k.startsWith('face.')&&!/(Color|Style|Brightness|resting|Asymmetry|blink|gaze|Expression|expression|hair|templeRecession|widowPeak)/.test(k));
summary.defaultExtraCounts={faceOnlyGeneCount:faceOnlyKeys.length,faceOnlyEndpoint:rows.filter(r=>faceOnlyKeys.some(k=>r.flat[k]===0||r.flat[k]===1)).length,nostrilWiderThanMouth:rows.filter(r=>r.metrics.nostrilOuterMouthWidth>1).length,neckWiderThanJaw:rows.filter(r=>r.metrics.neckJaw>1).length,mouthWidthClamped:rows.filter(r=>r.metrics.mouthWidthFaceWidth>=.57-1e-10).map(r=>r.seed)};
summary.correlation=Object.fromEntries(pairs.map(([a,b])=>[a+' vs '+b,pearson(rows.map(r=>r.flat[a]),rows.map(r=>r.flat[b]))]));
const featureKeys=['jawCheek','neckCheek','headWidthHeight','eyeGapEyeWidth','mouthWidthFaceWidth','eyeBrowGap','noseWidthMouthWidth'];
const distance=r=>featureKeys.reduce((s,k)=>s+((r.metrics[k]-summary.runs[1].metrics[k].median)/summary.runs[1].metrics[k].sd)**2,0);
summary.representatives=rows.slice().sort((a,b)=>distance(a)-distance(b)).slice(0,6).map(r=>({seed:r.seed,distance:distance(r),metrics:r.metrics,adjustments:r.adjustments}));
summary.severeAdjustments=rows.slice().sort((a,b)=>b.metrics.adjustedFeatureDeltaSum-a.metrics.adjustedFeatureDeltaSum).slice(0,6).map(r=>({seed:r.seed,metrics:r.metrics,adjustments:r.adjustments}));
summary.requestedAsymmetrySideToSideH=Object.fromEntries([
 ['eyeHeightAsymmetryGene',.0018],['browHeightAsymmetryGene',.002],['mouthCornerAsymmetryGene',.0016],['earAsymmetryGene',.0019]
].map(([key,scale])=>[key,stats(rows.map(r=>(r.flat['face.'+key]*2-1)*scale))]));
fs.writeFileSync(path.join(outDir,'genome_distribution_summary.json'),JSON.stringify(summary,null,2));
fs.writeFileSync(path.join(outDir,'genome_distribution_rows.json'),JSON.stringify(rows));
const compact={date:summary.date,generatorVersion:summary.generatorVersion,environment:summary.environment,sample:summary.sample,files:summary.files,
 runs:Object.fromEntries(Object.entries(summary.runs).map(([k,v])=>[k,{N:v.N,repeatable:v.repeatable,anyAdjustment:v.anyAdjustment,chinClamp:v.chinClamp.count,anyGeometryGeneEndpoint:v.anyGeometryGeneEndpoint,adjustments:Object.fromEntries(Object.entries(v.adjustments).map(([k,v])=>[k,{count:v.count,delta:v.delta,largest:v.largest[0]}]))}])),
 defaultExtraCounts:summary.defaultExtraCounts,defaultMetrics:summary.runs[1].metrics,
 requestedAsymmetrySideToSideH:summary.requestedAsymmetrySideToSideH,
 representatives:summary.representatives.map(r=>r.seed),severeAdjustments:summary.severeAdjustments.map(r=>({seed:r.seed,adjustments:r.adjustments})),extremes:summary.runs[1].extremes,correlation:summary.correlation
};
fs.writeFileSync(path.join(__dirname,'../data/genome_distribution_summary.json'),JSON.stringify(compact,null,2));
console.log(JSON.stringify({sample:summary.sample,runs:Object.fromEntries(Object.entries(compact.runs).map(([k,v])=>[k,{repeatable:v.repeatable,N:v.N,adjustments:Object.fromEntries(Object.entries(v.adjustments).map(([k,v])=>[k,v.count]))}])),representatives:compact.representatives,defaultExtraCounts:compact.defaultExtraCounts,requestedAsymmetrySideToSideH:summary.requestedAsymmetrySideToSideH},null,2));
