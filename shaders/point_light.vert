#version 450
const vec2 OFFSET[6] = vec2[](
    vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0),
    vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0)
);

layout(location=0) out vec2 fragOffset;

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseView;
    mat4 lightViewProj;      // ===== Shadow Map: 光源的 View*Proj 矩阵 =====
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
} ubo;

layout(push_constant) uniform Push {
    vec4 position;
    vec4 color;
    float radius;
} push;

void main() {
    fragOffset = OFFSET[gl_VertexIndex];
    
    // 获取相机世界坐标
    vec3 cameraPos = ubo.inverseView[3].xyz;
    vec3 toCamera = cameraPos - push.position.xyz;
    float len = length(toCamera);
    if (len < 0.001) {
        // 相机与光源重合，避免除零
        toCamera = vec3(0,0,1);
    } else {
        toCamera = normalize(toCamera);
    }
    
    // 计算广告牌的局部基向量（世界 Y 轴向上）
    vec3 right, up;
    const vec3 worldUp = vec3(0,1,0);
    if (abs(dot(toCamera, worldUp)) > 0.9999) {
        // 视线与 Y 轴平行，改用 Z 轴作为临时上方向
        vec3 tempUp = vec3(0,0,1);
        right = normalize(cross(tempUp, toCamera));
        up = cross(toCamera, right);
    } else {
        right = normalize(cross(worldUp, toCamera));
        up = cross(toCamera, right);
    }
    
    vec3 worldPos = push.position.xyz + (right * fragOffset.x + up * fragOffset.y) * push.radius;
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * vec4(worldPos, 1.0);
    
    // 如果你的投影矩阵包含了 Y 翻转，且导致广告牌上下颠倒，可以解除下面一行的注释
    // gl_Position.y = -gl_Position.y;
}
