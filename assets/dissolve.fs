#version 330

// Input vertex attributes
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float dissolveFactor;

// Output fragment color
out vec4 finalColor;

// Simple pseudo-random generator
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

// Simple noise function
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f*f*(3.0-2.0*f);
    return mix(mix(hash(i + vec2(0.0,0.0)), hash(i + vec2(1.0,0.0)), u.x),
               mix(hash(i + vec2(0.0,1.0)), hash(i + vec2(1.0,1.0)), u.x), u.y);
}

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    if (texelColor.a < 0.1) discard;

    vec2 noiseCoord = fragTexCoord * 50.0;
    float n = noise(noiseCoord);

    // Height-based threshold (0.0 at top, 1.0 at bottom)
    float threshold = dissolveFactor * 1.3 - (1.0 - fragTexCoord.y) * 0.3;

    if (n < threshold) {
        discard;
    }

    float border = 0.05;
    if (n < threshold + border) {
        finalColor = vec4(1.0, 0.4, 0.0, texelColor.a) * colDiffuse * fragColor * 2.0;
    } else {
        finalColor = texelColor * colDiffuse * fragColor;
    }
}
