(function () {
  'use strict';
  const R=window.RTS,M=R.Math,V=()=>new THREE.Vector3();
  function hermiteStep(p,v,target,time,dt) {
    const t=M.clamp(dt/Math.max(.001,time),0,1),t2=t*t,t3=t2*t;
    const h00=2*t3-3*t2+1,h10=t3-2*t2+t,h01=-2*t3+3*t2;
    const d00=(6*t2-6*t)/time,d10=3*t2-4*t+1,d01=(-6*t2+6*t)/time;
    for(const key of ['x','z']) {
      const a=p[key],b=v[key];
      p[key]=h00*a+h10*time*b+h01*target[key];
      v[key]=d00*a+d10*b+d01*target[key];
    }
  }
  /** Contact-space swing planning. Regular walking is distance-phased. Only a
   * finishing swing or a stationary stance adjustment uses a short local clock.
   * Never releases both feet to widen a stationary stance. */
  R.BipedFeet=class BipedFeet {
    constructor(anatomy) {
      this.A=anatomy;this.tracks=[0,1].map(()=>({active:false,lastSwing:false,p:V(),v:V(),end:V(),time:0,duration:.3}));
      this.adjust=null;this.replants=0;this._p=V();this._q=new THREE.Quaternion();this._off=V();this._contactOffset=V();
      this._heightCache={terrain:null,heights:null,ix:-1,iz:-1,a:0,b:0,c:0,d:0};
    }
    reset() {for(let i=0;i<this.tracks.length;i++){const t=this.tracks[i];t.active=false;t.lastSwing=false;}this.adjust=null;}
    ankleAtContact(i,anim,out) {
      const c=anim.footContacts[i];
      anim.rig.root.getWorldQuaternion(this._q);this._q.multiply(anim.footQ[i]);
      this._contactOffset.copy(c.local).applyQuaternion(this._q);
      return out.copy(c.point).sub(this._contactOffset);
    }
    update(pose,anim,context,dt) {
      if(!(dt>0)||context.treadmill||!anim.contactsEnabled(context)){this.reset();return;}
      const root=anim.rig.root,H=this.A.height,g=anim.locomotion.gait;
      if(context.speed<.03 && !anim.settling) {
        for(let i=0;i<this.tracks.length;i++){const t=this.tracks[i];t.active=false;t.lastSwing=false;}
        this.adjustStance(pose,anim,context,dt);return;
      }
      this.adjust=null;
      for(let i=0;i<2;i++) {
        const tr=this.tracks[i],c=anim.footContacts[i];
        const recoveryZ=anim.footSamples[i].recoveryZ||0;
        const phase=R.GaitProfile.wrap01(anim.phase+i*.5),swing=phase>=g.duty;
        const started=swing&&!tr.lastSwing;
        if(started) {
          tr.p.copy(pose.feet[i]);tr.p.z-=recoveryZ;
          root.localToWorld(tr.p.multiplyScalar(H));
          if(c.active)this.ankleAtContact(i,anim,tr.p);
          tr.v.set(0,0,0);tr.time=0;
          tr.duration=M.clamp((1-g.duty)*g.cycleM/Math.max(.1,context.speed),.12,.70);tr.active=true;
        }
        if(tr.active) {
          tr.time+=dt;
          const u=M.clamp((phase-g.duty)/(1-g.duty),0,1);
          let remaining=Math.max(.001,tr.duration*(1-u));
          if(!swing)remaining=dt;
          const front=g.cycleM*g.duty*.5;
          this._p.copy(pose.feet[i]).multiplyScalar(H);
          this._p.z=anim.bipedProfile.footZ+front;
          root.localToWorld(this._p);
          // Predict landing in world space; changes are replanned from current
          // position AND velocity, never by restarting the swing at a new seed.
          this._off.set(0,0,Math.max(0,context.speed)*remaining);
          root.getWorldQuaternion(this._q);this._off.applyQuaternion(this._q);this._p.add(this._off);
          if(tr.time<=dt*1.01)tr.end.copy(this._p);
          else tr.end.lerp(this._p,1-Math.exp(-6*dt));
          // p belongs to the PREVIOUS frame; remaining is measured from the
          // current one. Omitting dt makes the foot arrive early at high speed.
          // A new track is already sampled at this frame's phase.
          if(!started)hermiteStep(tr.p,tr.v,tr.end,swing?Math.max(dt,remaining+dt):dt,dt);
          this._p.copy(tr.p);this._p.y=context.surface.getHeightAtCached?context.surface.getHeightAtCached(this._p.x,this._p.z,this._heightCache):context.surface.getHeightAt(this._p.x,this._p.z);
          root.worldToLocal(this._p);
          pose.feet[i].x=this._p.x/H;pose.feet[i].z=this._p.z/H+recoveryZ;
          if(!swing){tr.active=false;pose.footPlant[i]=1;pose.footLift[i]=0;pose.feet[i].y=this.A.ankleHeight/H;}
        }
        tr.lastSwing=swing;
      }
    }
    adjustStance(pose,anim,context,dt) {
      const root=anim.rig.root,H=this.A.height;
      if(!this.adjust) {
        for(let i=0;i<2;i++) {
          const c=anim.footContacts[i],other=anim.footContacts[1-i];
          if(!c.active||!other.locked)continue;
          root.localToWorld(this._p.copy(pose.feet[i]).multiplyScalar(H));
          this.ankleAtContact(i,anim,this._off);
          if(Math.hypot(this._p.x-this._off.x,this._p.z-this._off.z)<H*.018)continue;
          this.adjust={i,start:this._off.clone(),end:this._p.clone(),time:0,duration:.32};
          c.release();this.replants++;break;
        }
      }
      const a=this.adjust;if(!a)return;
      a.time+=dt;const t=M.clamp(a.time/a.duration,0,1),w=R.GaitProfile.smooth5(t);
      this._p.copy(a.start).lerp(a.end,w);
      this._p.y=context.surface.getHeightAtCached?context.surface.getHeightAtCached(this._p.x,this._p.z,this._heightCache):context.surface.getHeightAt(this._p.x,this._p.z);
      root.worldToLocal(this._p);
      const lift=H*.019*16*t*t*(1-t)*(1-t);
      pose.feet[a.i].set(this._p.x/H,(this.A.ankleHeight+lift)/H,this._p.z/H);
      pose.footLift[a.i]=lift/H;pose.footPlant[a.i]=t>=1?1:0;pose.support[a.i]=t>=1?1:0;
      if(t>=1)this.adjust=null;
    }
  };
})();
