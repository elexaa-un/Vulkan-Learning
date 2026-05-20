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
    mat4 lightViewProj;      // ===== Shadow Map: ¹âÔ´µÄ View*Proj ¾ØÕó =====
    vec4 ambientColor;
    int numLights;
    PointLight pointLights[10];
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
