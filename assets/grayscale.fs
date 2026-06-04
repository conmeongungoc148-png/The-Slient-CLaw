#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Fetch texel color
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Calculate grayscale value using NTSC formula
    float gray = dot(texelColor.rgb, vec3(0.299, 0.587, 0.114));
    
    // Set final color
    finalColor = vec4(gray, gray, gray, texelColor.a) * colDiffuse * fragColor;
}
