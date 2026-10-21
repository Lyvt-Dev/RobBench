#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(push_constant) uniform Push {mat4 mvp;vec4 rotation;vec4 color;} p;
layout(location=0) out vec3 worldNormal;
void main(){gl_Position=p.mvp*vec4(position,1);worldNormal=normal+2*cross(p.rotation.xyz,cross(p.rotation.xyz,normal)+p.rotation.w*normal);}
