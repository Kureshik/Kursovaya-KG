uniform sampler2D tex;
uniform float time;
uniform vec3 tint;
uniform float alpha;

varying vec2 texCoord;
varying vec3 Normal;

void main(void)
{
    vec4 sampleColor = texture2D(tex, texCoord);
    vec3 n = normalize(Normal);
    float rim = pow(1.0 - abs(n.z) * 0.55, 2.0);
    float pulse = 0.78 + 0.22 * sin(time * 6.0 + texCoord.x * 12.0);
    vec3 color = sampleColor.rgb * tint * (1.15 + rim * 1.35) * pulse;
    gl_FragColor = vec4(color, sampleColor.a * alpha);
}
