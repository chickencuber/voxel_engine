#version 330 core
in vec2 TexCoord;
flat in int isSolidColor;


out vec4 FragColor;

uniform sampler2D texture1; // your texture sampler
uniform vec2 screenSize;  // Pass viewport size from your app


void main() {
    vec4 color = vec4(0.0, 0.56, 0.78, 1.0);
    if(isSolidColor == 0){
        color = texture(texture1, TexCoord);
    }
    
    // Define crosshair size in pixels
    float crosshairHalfSize = 13.0;
    float crosshairWidth = 2;
    
    vec2 center = screenSize * 0.5;
    
    // If pixel inside horizontal or vertical line of crosshair (+ shape)
    bool inHorizontal = abs(gl_FragCoord.y - center.y) < crosshairWidth && abs(gl_FragCoord.x - center.x) < crosshairHalfSize;
    bool inVertical = abs(gl_FragCoord.x - center.x) < crosshairWidth && abs(gl_FragCoord.y - center.y) < crosshairHalfSize;
    
    if (inHorizontal || inVertical) {
        color.rgb = vec3(1.0) - color.rgb;  // invert colors
    }
    
    FragColor = color;
}


