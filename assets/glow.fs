#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Custom uniform values
uniform vec4 glowColor;
uniform float glowRadius; // in pixels (e.g. 6.0)

// Output fragment color
out vec4 finalColor;

void main()
{
    // Get texel size
    vec2 texelSize = 1.0 / textureSize(texture0, 0);
    vec4 texelColor = texture(texture0, fragTexCoord);

    // Calculate glow alpha from neighbor pixels
    float alphaGlow = 0.0;
    int samples = 0;
    
    // 7x7 sampling grid for a smooth glow
    for (float x = -3.0; x <= 3.0; x += 1.0)
    {
        for (float y = -3.0; y <= 3.0; y += 1.0)
        {
            // Calculate sample offset based on radius
            vec2 offset = vec2(x, y) * texelSize * (glowRadius / 3.0);
            float a = texture(texture0, fragTexCoord + offset).a;
            
            // Weight samples by distance (closer = stronger)
            float dist = length(vec2(x, y));
            float weight = max(0.0, 1.0 - (dist / 4.242)); // 4.242 is length(3,3)
            alphaGlow += a * weight;
            samples++;
        }
    }
    
    // Normalize and scale up the glow intensity
    alphaGlow = (alphaGlow / float(samples)) * 4.0;
    alphaGlow = clamp(alphaGlow, 0.0, 1.0);

    // Glow color with calculated alpha
    vec4 finalGlowColor = vec4(glowColor.rgb, alphaGlow * glowColor.a);
    
    // Tinted original sprite color
    vec4 originalColor = texelColor * fragColor * colDiffuse;
    
    // Blend: if the original pixel is transparent, we see the glow.
    // If the original pixel is solid, we see the original texture color.
    finalColor = mix(finalGlowColor, originalColor, originalColor.a);
}
