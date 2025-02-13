#dv_include "uniforms.inc"
#dv_include "samplers.inc"
#dv_include "transform.inc"

#ifdef VSM_SHADOW
    VS_OUT_FS_IN(vec4 vTexCoord)
#else
    VS_OUT_FS_IN(vec2 vTexCoord)
#endif


#ifdef COMPILEVS
void main()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    #ifdef VSM_SHADOW
        vTexCoord = vec4(GetTexCoord(iTexCoord), gl_Position.z, gl_Position.w);
    #else
        vTexCoord = GetTexCoord(iTexCoord);
    #endif
}
#endif // def COMPILEVS


#ifdef COMPILEFS
void main()
{
    #ifdef ALPHAMASK
        float alpha = texture(sDiffMap, vTexCoord.xy).a;
        if (alpha < 0.5)
            discard;
    #endif

    #ifdef VSM_SHADOW
        float depth = vTexCoord.z / vTexCoord.w * 0.5 + 0.5;
        gl_FragColor = vec4(depth, depth * depth, 1.0, 1.0);
    #else
        gl_FragColor = vec4(1.0);
    #endif
}
#endif // def COMPILEFS
