#ifndef GENOMES_CHARACTER_COMMON_HLSLI
#define GENOMES_CHARACTER_COMMON_HLSLI

float3 characterLight(float3 base_color,
                      float3 normal,
                      float roughness,
                      float specular,
                      float4 key_direction_intensity,
                      float4 key_color,
                      float4 fill_direction_intensity,
                      float4 fill_color,
                      float4 hemisphere_sky,
                      float4 hemisphere_ground) {
    float3 key_direction = normalize(key_direction_intensity.xyz);
    float3 fill_direction = normalize(fill_direction_intensity.xyz);
    float hemisphere_factor = saturate(normal.y * 0.5 + 0.5);
    float3 hemisphere = lerp(hemisphere_ground.rgb, hemisphere_sky.rgb,
                             hemisphere_factor) *
                        lerp(hemisphere_ground.a, hemisphere_sky.a,
                             hemisphere_factor);
    float key = saturate(dot(normal, key_direction)) * key_direction_intensity.w;
    float fill = saturate(dot(normal, fill_direction)) * fill_direction_intensity.w;
    float3 view_direction = normalize(float3(0.0, 0.35, 1.0));
    float3 half_vector = normalize(key_direction + view_direction);
    float highlight = pow(saturate(dot(normal, half_vector)),
                          lerp(10.0, 64.0, 1.0 - roughness));
    return base_color * (hemisphere + 0.58 * key * key_color.rgb +
                         0.20 * fill * fill_color.rgb) +
           specular * highlight;
}

#endif
