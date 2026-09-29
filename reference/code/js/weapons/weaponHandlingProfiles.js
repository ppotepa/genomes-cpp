(function () {
  'use strict';
  const R = window.RTS, M = R.Math;
  const V = (x=0,y=0,z=0) => new THREE.Vector3(x,y,z);
  const clamp = x => M.clamp(x,0,1);
  // Art-directed poses. Positions are fractions of the actual arm length,
  // relative to the animated shoulder; angles are radians. No weapon scaling.
  const profiles = {
    PISTOL_1H: {
      low:[-.045,-.82,.14], high:[0,-.13,.84], run:[-.01,-.39,.28],
      lowR:[1.08,0,.09], highR:[0,0,.025], runR:[.72,0,.10],
      sway:.045, bob:.011, pole:[-.34,-.40,.15], fingers:'PISTOL', aim:true,
      prone:[-.105,.052,.43], proneR:[.08,0,.03], kick:1, recovery:19
    },
    PISTOL_2H: {
      low:[.28,-.49,.36], high:[.30,-.12,.78], run:[.20,-.33,.27],
      lowR:[.63,0,0], highR:[0,0,0], runR:[.65,0,.025],
      sway:.014, bob:.006, pole:[-.27,-.35,.25], fingers:'PISTOL', aim:true,
      prone:[-.065,.058,.46], proneR:[.06,0,0], kick:.72, recovery:25
    },
    KNIFE_1H: {
      low:[-.09,-.84,.03], high:[-.035,-.49,.39], run:[-.02,-.44,.20],
      lowR:[.08,0,-2.95], highR:[.25,.05,-.28], runR:[.40,0,-1.00],
      sway:.052, bob:.009, pole:[-.38,-.55,.05], fingers:'KNIFE', aim:false,
      prone:[-.13,.065,.35], proneR:[1.20,0,-.42], kick:0, recovery:22
    },
    GRENADE_OVAL_1H: {
      low:[-.06,-.70,.12], high:[.10,-.36,.38], run:[.03,-.38,.22],
      lowR:[.12,0,-.16], highR:[-.12,.12,-.12], runR:[.22,0,-.16],
      sway:.027, bob:.008, pole:[-.32,-.42,.22], fingers:'SPHERE', aim:false,
      prone:[-.12,.067,.36], proneR:[.15,0,-.14], kick:0, recovery:22
    },
    GRENADE_CYLINDER_1H: {
      low:[-.045,-.67,.15], high:[.07,-.32,.36], run:[.025,-.36,.24],
      lowR:[.04,0,-.045], highR:[-.04,.08,-.03], runR:[.14,0,-.06],
      sway:.022, bob:.006, pole:[-.30,-.40,.24], fingers:'CYLINDER', aim:false,
      prone:[-.12,.073,.38], proneR:[.08,0,-.035], kick:0, recovery:22
    }
  };
  for(const [id,p] of Object.entries(profiles)) {
    for(const value of Object.values(p))if(Array.isArray(value))Object.freeze(value);
    profiles[id]=Object.freeze({...p,id});
  }
  function resolve(def,grip='1H') {
    if(!def)return null;
    if(def.kind==='pistol')return profiles[grip==='2H'?'PISTOL_2H':'PISTOL_1H'];
    if(def.kind==='knife')return profiles.KNIFE_1H;
    if(def.kind==='grenade')return profiles[def.gripShape==='CYLINDER'?'GRENADE_CYLINDER_1H':'GRENADE_OVAL_1H'];
    return null; // Long-gun handling stays in WeaponController.
  }
  R.WeaponHandlingProfiles=class WeaponHandlingProfiles {
    constructor(anatomy) {
      this.A=anatomy;
      this.arm=anatomy.points['upperArm.R'].distanceTo(anatomy.points['foreArm.R'])+
        anatomy.points['foreArm.R'].distanceTo(anatomy.points['hand.R']);
      this.offset=V();this.shoulder=V();this.chest=V();this.low=V();
      this.q=new THREE.Quaternion();this.r=new THREE.Quaternion();this.euler=new THREE.Euler();
    }
    static resolve(def,grip){return resolve(def,grip);}
    static get(id){return profiles[id]||null;}
    sample(profile,controller,out,ready) {
      const u=controller.unit,L=this.arm,H=this.A.height,g=u.locomotion;
      const depth=clamp(g.actualCrouch||0),prone=clamp(u.animator.current.prone||0);
      const running=clamp((g.runWeight||0)*.50+(g.sprintWeight||0)*.50);
      const movement=R.GaitProfile.smooth5(Math.max(0,u.speed)/.65);
      const tuck=running*movement*(1-ready);
      const phase=u.animator.phase*Math.PI*2,time=u.animator.time;
      ready=clamp(ready);
      this.offset.fromArray(profile.low).lerp(this.low.fromArray(profile.high),ready);
      this.offset.lerp(this.low.fromArray(profile.run),tuck);
      this.offset.y+=depth*.13*(1-ready);
      this.offset.z+=depth*.04*(1-ready);
      this.offset.multiplyScalar(L);
      const freedom=(1-ready*.94)*(1-tuck*.65)*movement;
      this.offset.z+=Math.cos(phase)*L*profile.sway*freedom;
      this.offset.y+=Math.sin(phase*2)*L*profile.bob*freedom;
      this.offset.y+=Math.sin(time*1.4)*H*.0012;
      this.q.setFromEuler(this.euler.set(...profile.lowR,'YXZ'));
      this.r.setFromEuler(this.euler.set(...profile.highR,'YXZ'));this.q.slerp(this.r,ready);
      this.r.setFromEuler(this.euler.set(...profile.runR,'YXZ'));this.q.slerp(this.r,tuck);
      if(profile.aim) {
        // Both the arm's extension and the weapon follow aim. Rotating only the
        // prop leaves an implausibly fixed wrist when looking up or sideways.
        this.r.setFromEuler(this.euler.set(-controller.aimPitch*ready,controller.aimYaw*ready,0,'YXZ'));
        this.offset.applyQuaternion(this.r);this.q.premultiply(this.r);
      }
      controller.rig.modelPoint('upperArm.R',this.shoulder);
      out.p.copy(this.shoulder).add(this.offset);out.q.copy(this.q);
      if(prone>0) {
        controller.rig.modelPoint('chest',this.chest);
        this.low.set(this.chest.x+profile.prone[0]*H,this.chest.y+profile.prone[1],this.chest.z+profile.prone[2]*L);
        this.r.setFromEuler(this.euler.set(profile.proneR[0]-(profile.aim?controller.aimPitch*ready:0),
          profile.proneR[1]+(profile.aim?controller.aimYaw*ready:0),profile.proneR[2],'YXZ'));
        out.p.lerp(this.low,prone);out.q.slerp(this.r,prone);
      }
      const swing=Math.sin(phase)*profile.sway*.7*freedom;
      this.r.setFromEuler(this.euler.set(swing,0,swing*.18,'XYZ'));out.q.multiply(this.r).normalize();
      return out;
    }
  };
})();
