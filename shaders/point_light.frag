#version 450
layout(location=0) in vec2 fragOffset;
layout (location=0) out vec4 outColor;
struct PointLight{
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

    // ===== 新增：风力参数（必须与C++端GlobalUbo对齐）=====
    float windTime;
    float windStrength;
    float windSpeed;
    float windDirectionX;
    float windDirectionZ;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;
layout(push_constant) uniform Push{
vec4 position;
vec4 color;
float radius;
}push;

const float M_PI = 3.1415926384; 
void main(){
    float dis=sqrt(dot(fragOffset,fragOffset));
    if(dis>1.0){
        discard;
    }
    outColor = vec4(push.color.xyz,0.5*(cos(dis*M_PI)+1.0));
}
