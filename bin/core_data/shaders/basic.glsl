#dv_include "uniforms.inc"
#dv_include "samplers.inc"
#dv_include "transform.inc"

#if defined(DIFFMAP) || defined(ALPHAMAP)
    VS_OUT_FS_IN(vec2 vTexCoord)
#endif
#ifdef VERTEXCOLOR
    VS_OUT_FS_IN(vec4 vColor)
#endif


#ifdef COMPILEVS
void main()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);

    #ifdef DIFFMAP
        vTexCoord = iTexCoord;
    #endif
    #ifdef VERTEXCOLOR
        vColor = iColor;
    #endif
}
#endif // def COMPILEVS


#ifdef COMPILEFS
void main()
{
    vec4 diffColor = cMatDiffColor;

    #ifdef VERTEXCOLOR
        diffColor *= vColor;
    #endif

    #if (!defined(DIFFMAP)) && (!defined(ALPHAMAP))
        gl_FragColor = diffColor;
    #endif
    #ifdef DIFFMAP
        vec4 diffInput = texture(sDiffMap, vTexCoord);
        #ifdef ALPHAMASK
            if (diffInput.a < 0.5)
                discard;
        #endif
        gl_FragColor = diffColor * diffInput;
    #endif
    #ifdef ALPHAMAP
        float alphaInput = texture(sDiffMap, vTexCoord).r;
        gl_FragColor = vec4(diffColor.rgb, diffColor.a * alphaInput);
    #endif
}
#endif // def COMPILEFS
