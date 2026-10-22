#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
layout(push_constant) uniform Push {mat4 mvp;vec4 normalX;vec4 normalY;vec4 normalZ;vec4 color;} p;
layout(location=0) out vec3 worldNormal;
layout(location=1) out vec3 vertexColor;
void main(){gl_Position=p.mvp*vec4(position,1);worldNormal=mat3(p.normalX.xyz,p.normalY.xyz,p.normalZ.xyz)*normal;vertexColor=color;}
