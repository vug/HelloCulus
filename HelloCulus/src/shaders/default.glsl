#version 410
out vec4 fragColor;

uniform float time = 0.0;
uniform vec3 ro = vec3(0, 0, 1.0);
uniform mat4 view = mat4(1.0);
uniform mat4 proj = mat4(1.0);
uniform float frustFovH = 1.7;
uniform float frustFovV = 1.2;
uniform int eyeNo = 0;

float radius = 1.;

float map(vec3 p)
{
    float d = distance(p, vec3(-1 + sin(time) * 2.0, 0, -5)) - radius;
    d = min(d, distance(p, vec3(2, 0, -3)) - radius);
    d = min(d, distance(p, vec3(-2, 0, -2)) - radius);
    d = min(d, p.y + 1.); // floor (?)
    return d;
}
vec3 calcNormal(vec3 p)
{
    vec2 e = vec2(1.0, -1.0) * 0.0005;
    return normalize(
        e.xyy * map(p + e.xyy) +
        e.yyx * map(p + e.yyx) +
        e.yxy * map(p + e.yxy) +
        e.xxx * map(p + e.xxx));
}

void main()
{
    vec2 res = vec2(1344, 1600);
    vec2 q = (1.0 * gl_FragCoord.xy - 0.5 * res) / res; // [-1, 1]
    vec3 rdFixed = normalize(vec3(q, -1));

    vec3 forward = normalize(view * vec4(0, 0, -1, 1)).xyz; // local forward
    vec3 u = normalize(cross(vec3(0, 1, 0), forward)); // local X
    vec3 v = normalize(cross(forward, u)); // local Y
    vec3 rd = normalize(-q.x * u + q.y * v + 1.5 * forward); // TODO: make zoom a parameter

    //float z = 0.1 / tan(frustFovV * 0.1);
    //vec3 rd = normalize((view * vec4(q.x, q.y, -z, 1.0)).xyz);

    float correction = 1.0;
    vec3 roc;
    if (eyeNo == 0)
    {
        roc = ro + u * correction;
    }
    if (eyeNo == 1)
    {
        roc = ro + u * correction;
    }

    float h, t = 1.;
    for (int i = 0; i < 256; i++)
    {
        h = map(roc + rd * t);
        t += h;
        if (h < 0.01)
            break;
    }
    if (h < 0.01)
    {
        vec3 p = roc + rd * t;
        vec3 normal = calcNormal(p);
        vec3 light = vec3(0, 3, 0);
        float dif = clamp(dot(normal, normalize(light - p)), 0., 1.);
        dif *= 5. / dot(light - p, light - p);
        fragColor = vec4(vec3(pow(dif, 0.4545)), 1);
    }
    else
    {
        fragColor = vec4(0, 0, 0, 1);
    }
}