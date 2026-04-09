#version 330 core
out vec4 FragColor;

// Interpolated final color from vertex shader (true Gouraud)
in vec4 aColor;

void main()
{
    FragColor = aColor;
}
