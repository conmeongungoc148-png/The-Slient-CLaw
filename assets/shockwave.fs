#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 center; // Shockwave center in UV coordinates (0.0 to 1.0)
uniform float time;  // Time variable to animate shockwave (radius)
uniform vec3 shockParams; // x: frequency/amount, y: amplitude, z: refraction/width

out vec4 finalColor;

void main()
{
    vec2 uv = fragTexCoord;
    vec2 texCoord = uv;
    
    // Account for aspect ratio if needed (assuming 16:9 for distance calc to make ring round)
    // Here we'll just keep it simple, but for a perfect circle we'd scale uv.y
    float aspect = 1280.0 / 720.0;
    vec2 aspectUV = vec2(uv.x * aspect, uv.y);
    vec2 aspectCenter = vec2(center.x * aspect, center.y);
    
    float dist = distance(aspectUV, aspectCenter);
    
    if (time > 0.0 && dist <= time + shockParams.z && dist >= time - shockParams.z) {
        float diff = (dist - time);
        float powDiff = 1.0 - pow(abs(diff * shockParams.x), shockParams.y);
        
        float diffTime = diff * powDiff;
        vec2 diffUV = normalize(uv - center);
        
        texCoord = uv + (diffUV * diffTime);
    }
    
    finalColor = texture(texture0, texCoord) * fragColor;
}
