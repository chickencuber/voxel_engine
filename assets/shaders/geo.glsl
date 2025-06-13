#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;

flat out int isSolidColor;
in vec2 inTexCoord[];  // input from vertex shader (array for 3 verts)
out vec2 TexCoord;   // output to fragment shader

void main() {

    EndPrimitive();
    for (int i = 0; i < 3; i++) {
        gl_Position = gl_in[i].gl_Position;
        TexCoord = inTexCoord[i];
        isSolidColor = 0;  // textured
        EmitVertex();
    }
    EndPrimitive();
    // extra solid color triangle
    gl_Position = vec4(-1.0, -1.0, 0.999999, 1.0);
    isSolidColor = 1;   // solid color
    EmitVertex();

    gl_Position = vec4(1, -1.0, 0.999999, 1.0);
    isSolidColor = 1;
    EmitVertex();

    gl_Position = vec4(0, 1, 0.999999, 1.0);
    isSolidColor = 1;
    EmitVertex();

}

