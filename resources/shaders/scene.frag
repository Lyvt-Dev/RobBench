#version 450
layout(location=0) in vec3 worldNormal;
layout(push_constant) uniform Push {mat4 mvp;vec4 rotation;vec4 color;} p;
layout(location=0) out vec4 outColor;
void main(){float light=.4+.6*max(dot(normalize(worldNormal),normalize(vec3(-.4,.8,.6))),0);outColor=vec4(p.color.rgb*light,1);}
