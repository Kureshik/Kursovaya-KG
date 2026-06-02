varying vec3 Normal;
varying vec3 Position;

uniform float time;
uniform vec3 baseColor;
uniform float glowStrength;

void main(void)
{
    vec3 n = normalize(Normal);
    float rim = pow(1.0 - abs(n.z) * 0.65, 2.0);
    float pulse = 0.72 + 0.28 * sin(time * 5.5 + Position.x * 1.7 + Position.y * 1.3);
    vec3 color = baseColor * (0.45 + glowStrength * rim) * pulse;
    gl_FragColor = vec4(color, 1.0);
}
