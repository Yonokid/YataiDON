uniform vec4 u_skinPosition[192];
uniform vec4 u_skinNormal[192];

void skinVertex(vec3 bindPosition, vec3 bindNormal, uvec4 indices, vec4 weights,
                out vec3 position, out vec3 normal)
{
    position = vec3(0.0);
    normal = vec3(0.0);
    for (int influence = 0; influence < 4; ++influence)
    {
        float weight = weights[influence];
        if (weight != 0.0)
        {
            int row = int(indices[influence]) * 3;
            vec4 p = vec4(bindPosition, 1.0);
            position += weight * vec3(dot(u_skinPosition[row], p), dot(u_skinPosition[row + 1], p),
                                      dot(u_skinPosition[row + 2], p));
            vec4 n = vec4(bindNormal, 0.0);
            normal += weight * vec3(dot(u_skinNormal[row], n), dot(u_skinNormal[row + 1], n),
                                    dot(u_skinNormal[row + 2], n));
        }
    }
}
