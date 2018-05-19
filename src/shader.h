const char *vsSource = R"(#line 1
    attribute vec3 aVertexPosition;
    attribute vec2 aTextureCoord;
    attribute vec3 aNormal;
    attribute vec3 aTangent;
    attribute vec4 aBoneIndex;
    attribute vec4 aBoneWeight;

    uniform mat4 uModelViewMatrix;
    uniform mat4 uProjectionMatrix;
    uniform mat4 uBones[8];

    varying vec2 vTextureCoord;
    varying vec3 vNormal;
    varying vec3 vTangent;
    varying vec3 toLightVector;
    varying vec3 toCameraVector;

    void main(void) {
        mat4 skinning = mat4(0.0);

        skinning += aBoneWeight.x * uBones[int(aBoneIndex.x)];
        skinning += aBoneWeight.y * uBones[int(aBoneIndex.y)];
        skinning += aBoneWeight.z * uBones[int(aBoneIndex.z)];
        skinning += aBoneWeight.w * uBones[int(aBoneIndex.w)];

        //skinning = mat4(1.0);

        vec4 positionRelativeToCam = uModelViewMatrix * skinning * vec4(aVertexPosition, 1.0);
        gl_Position = uProjectionMatrix * positionRelativeToCam;

        vTextureCoord.x = aTextureCoord.x;
        vTextureCoord.y = 1.0-aTextureCoord.y;

        vec3 norm = normalize((uModelViewMatrix * skinning * vec4(aNormal,0.0)).xyz);
        vec3 tang = normalize((uModelViewMatrix * skinning * vec4(aTangent, 0.0)).xyz);
        vec3 bitang = normalize(cross(norm, tang));

        mat3 toTangentSpace = mat3(
            tang.x, bitang.x, norm.x,
            tang.y, bitang.y, norm.y,
            tang.z, bitang.z, norm.z
        );

        //toLightVector = toTangentSpace * (lightPositionEyeSpace - positionRelativeToCam.xyz);
        toLightVector = toTangentSpace * vec3(-1, 0.5, 1);
        toCameraVector = toTangentSpace * (-positionRelativeToCam.xyz);

        vNormal = (uProjectionMatrix * uModelViewMatrix * skinning * vec4(aNormal, 0)).xyz;
        vTangent = (uBones[0]*vec4(aTangent,1.0)).xyz;
    })";



const char *fsSource = R"(#line 55
    varying vec2 vTextureCoord;
    varying vec3 vNormal;
    varying vec3 vTangent;
    varying vec3 toLightVector;
    varying vec3 toCameraVector;

    uniform sampler2D aoTexture;
    uniform sampler2D norTexture;
    uniform sampler2D hatTexture;
    uniform sampler2D fabricTexture;
    uniform bool uEyes;
    uniform bool uHat;

    vec3 blendSoftLight(vec3 base, vec3 blend) {
        return mix(
            sqrt(base) * (2.0 * blend - 1.0) + 2.0 * base * (1.0 - blend),
            2.0 * base * blend + base * base * (1.0 - 2.0 * blend),
            step(base, vec3(0.5))
        );
    }

    float fresnel_(vec3 normal, vec3 lightDir, float exponent) {
        return pow(max(dot(normal, lightDir), 0.0), exponent);
    }

    void main(void) {
        vec3 color = vec3(0.0);
        vec3 normal = normalize(vNormal);

        vec3 unitNormal = normalize(2.0 * texture2D(norTexture, vTextureCoord).rgb - 1.0);

        vec3 totalDiffuse;
        vec3 totalSpecular;

        if(uEyes) {
            totalDiffuse = vec3(0.0);
            totalSpecular = vec3(pow(dot(normal, normalize(vec3(0.1, 0.15, -1))), 200.0))*2.0;
        } else if (uHat) {
            float fresnel = fresnel_(normal, vec3(0, 0, -1), 2.0);
            color = blendSoftLight(texture2D(hatTexture, vTextureCoord).rgb, (texture2D(fabricTexture, vTextureCoord*2.0).rgb-0.6)*0.4+0.6);
            color = mix(color, color*0.7, fresnel);

            totalDiffuse = vec3(fresnel_(normal, vec3(0, 0, -1), 0.5));
            totalSpecular = vec3(0.0);

        } else {
            vec3 baseColor1 = vec3(1.0, 0.597202, 0.401978);
            vec3 baseColor2 = vec3(0.461568, 0.086268, 0.056358);
            float fresnel = fresnel_(normal, vec3(0, 0, -1), 1.3);
            float ao = texture2D(aoTexture, vTextureCoord).r;

            baseColor1 = blendSoftLight(baseColor1, vec3(ao));
            color = mix(baseColor2, baseColor1, fresnel);

            vec3 unitVectorToCamera = normalize(toCameraVector);
            vec3 unitLightVector = normalize(toLightVector);

            vec3 reflectedLightDirection = reflect(-unitLightVector, unitNormal);

            float nDotl = max(dot(unitNormal, unitLightVector)*0.5+0.5, 0.0);
            float specularFactor = max(dot(reflectedLightDirection, unitVectorToCamera), 0.0);

            float dampedFactor = pow(specularFactor, 200.0);
            totalDiffuse = vec3(nDotl)+0.5;
            totalSpecular = (dampedFactor * 1.0 * vec3(1.0));
        }


        gl_FragColor = vec4(totalDiffuse,1.0) * vec4(color, 1.0) + vec4(totalSpecular,1.0);
    })";
