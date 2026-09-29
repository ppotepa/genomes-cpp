#include <genomes/infantry/PostureProfile.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

namespace {
constexpr float keys[5][7] = {
    {0.0F, .985F, .000F, .010F, .004F, .003F, .003F},
    {.25F, .925F, .025F, .185F, .052F, .044F, .022F},
    {.50F, .810F, .060F, .385F, .085F, .071F, .030F},
    {.75F, .600F, .092F, .490F, .075F, .062F, .025F},
    {1.0F, .315F, .110F, .500F, .078F, .066F, .028F}};

float smooth5(float value) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
}

float smoothRange(float edge0, float edge1, float value) noexcept {
    if (edge0 == edge1) return value >= edge1 ? 1.0F : 0.0F;
    return smooth5((value-edge0)/(edge1-edge0));
}

float slope(std::uint32_t row, std::uint32_t column) noexcept {
    if (row == 0) return (keys[1][column] - keys[0][column]) * 4.0F;
    if (row == 4) return (keys[4][column] - keys[3][column]) * 4.0F;
    const float left = (keys[row][column] - keys[row - 1][column]) * 4.0F;
    const float right = (keys[row + 1][column] - keys[row][column]) * 4.0F;
    return left * right <= 0.0F ? 0.0F : 2.0F * left * right / (left + right);
}
} // namespace

float PostureProfile::curve(float crouch, std::uint32_t column) noexcept {
    if (column < 1 || column > 6) return 0.0F;
    const float c = std::clamp(std::isfinite(crouch) ? crouch : 0.0F, 0.0F, 1.0F);
    const auto index = static_cast<std::uint32_t>(std::min(3.0F, std::floor(c * 4.0F)));
    const float t = (c - keys[index][0]) * 4.0F;
    const float t2 = t * t, t3 = t2 * t;
    return (2*t3-3*t2+1)*keys[index][column] + (t3-2*t2+t)*.25F*slope(index,column) +
           (-2*t3+3*t2)*keys[index+1][column] + (t3-t2)*.25F*slope(index+1,column);
}

PostureSample PostureProfile::sample(float crouch) noexcept {
    const float value = std::clamp(std::isfinite(crouch) ? crouch : 0.0F, 0.0F, 1.0F);
    return {value, curve(value, 1), curve(value, 3), curve(value, 3),
            -curve(value, 4), smooth5(value)};
}

PostureSample PostureProfile::sample(float crouch, const BodyPhenotype& body) noexcept {
    PostureSample out=sample(crouch);
    const float c=out.crouch,H=body.height,L=body.anatomy_leg_length;
    constexpr float ankle_ratio=.045F, hip_offset_ratio=.015F;
    out.pelvis_pitch=curve(c,3);
    out.hip_y=ankle_ratio*H+L*curve(c,1)+std::cos(out.pelvis_pitch)*hip_offset_ratio*H;
    out.hip_z=-L*curve(c,2)+std::sin(out.pelvis_pitch)*hip_offset_ratio*H;
    out.lower_pitch=curve(c,4);out.upper_pitch=curve(c,5);out.chest_pitch=curve(c,6);
    const float bulk=std::max(0.0F,body.leg_thickness_scale-1.0F);
    out.stance_half=.052F*body.hip_width_scale*H+
        (.004F+.025F*smooth5(c)+bulk*.012F)*H;
    out.foot_z=L*.025F*c;
    out.foot_yaw=.05F+.20F*smooth5(c);
    out.knee_half=out.stance_half+H*(.018F+.030F*c);
    return out;
}

float PostureProfile::speedLimit(float crouch, const BodyPhenotype& p) noexcept {
    const float values[5] = {p.run_speed, p.run_speed*.68F, p.walk_speed*.86F,
                             p.crouch_speed*.68F, p.crouch_speed*.28F};
    const float c = std::clamp(crouch, 0.0F, 1.0F);
    const auto index = static_cast<std::uint32_t>(std::min(3.0F, std::floor(c*4.0F)));
    const float t = smooth5((c - static_cast<float>(index)*.25F)*4.0F);
    return values[index] + (values[index+1]-values[index])*t;
}

float PostureProfile::depthAtSpeed(float speed, const BodyPhenotype& p) noexcept {
    if (speed <= speedLimit(1.0F, p)) return 1.0F;
    if (speed >= speedLimit(0.0F, p)) return 0.0F;
    float lo=0.0F, hi=1.0F;
    for (int i=0;i<14;++i) {
        const float mid=(lo+hi)*.5F;
        if (speedLimit(mid,p)>=speed) lo=mid; else hi=mid;
    }
    return lo;
}

GaitSample PostureProfile::gait(float crouch, float speed, const BodyPhenotype& p) noexcept {
    const float c=std::clamp(crouch,0.0F,1.0F), v=std::max(0.0F,speed);
    const float ratio=v/std::max(.1F,p.speed_multiplier);
    const float run=smooth5((ratio-1.75F)/1.05F)*(1.0F-smooth5((c-.16F)/.40F));
    const float sprint=run*smooth5((v/std::max(.1F,p.run_speed)-.70F)/.30F);
    const float shortening=1.0F+(.27F-1.0F)*smooth5(c);
    const double max_cycle=(static_cast<double>(p.anatomy_leg_length)*1.50+
        (static_cast<double>(p.anatomy_leg_length)*2.50-
         static_cast<double>(p.anatomy_leg_length)*1.50)*run)*shortening;
    const float cadence_base=(1.05F+(1.92F-1.05F)*run)*(1.0F+(.82F-1.0F)*c);
    GaitSample out{};
    out.cycle_m=std::max(.11*static_cast<double>(p.height),
                         std::min(max_cycle,static_cast<double>(v/cadence_base)));
    out.run=run; out.sprint=sprint;
    const float walk_duty=.62F+(.76F-.62F)*c;
    out.duty=walk_duty+(.42F-walk_duty)*run;
    out.duty += (.38F-out.duty)*sprint;
    const float walk_lift=.048F+(.012F-.048F)*c;
    out.lift_m=(walk_lift+(.100F-walk_lift)*run);
    out.lift_m=(out.lift_m+(.180F-out.lift_m)*sprint)*p.height;
    out.amplitude=smooth5(v/.20F);
    out.cadence=static_cast<float>(v/out.cycle_m);
    return out;
}

FootSample PostureProfile::sampleLowContact(double phase, float duty,
                                             double cycle_distance, float clearance,
                                             float sprint) noexcept {
    double t=std::fmod(phase,1.0);
    if (t<0.0) t+=1.0;
    const float d=std::clamp(duty, 1.0e-6F, 1.0F-1.0e-6F);
    const double half=cycle_distance*d*.5;
    FootSample out{};
    out.stance=t<d;
    out.swing=out.stance ? 0.0F : static_cast<float>((t-d)/(1.0F-d));
    if (out.stance) {
        const float u=static_cast<float>(t/d);
        out.z=static_cast<float>(half-cycle_distance*t);
        out.plant=smoothRange(0.0F,.075F,u)*(1.0F-smoothRange(.86F,1.0F,u));
        out.support=out.plant;
        return out;
    }
    const float u=out.swing,u2=u*u,u3=u2*u;
    const double tangent=-cycle_distance*(1.0F-d);
    out.z=static_cast<float>((2*u3-3*u2+1)*(-half)+(u3-2*u2+u)*tangent+
          (-2*u3+3*u2)*half+(u3-u2)*tangent);
    const float arc=16*u2*(1-u)*(1-u);
    const float v=u-sprint*.14F*arc,v2=v*v,v3=v2*v;
    const float recovery=static_cast<float>((2*v3-3*v2+1)*(-half)+(v3-2*v2+v)*tangent+
                         (-2*v3+3*v2)*half+(v3-v2)*tangent);
    out.recovery_z=recovery-out.z;
    out.z=recovery;
    out.lift=clearance*arc*(1.0F+sprint*1.6F*(.5F-u));
    return out;
}

} // namespace genomes::infantry
