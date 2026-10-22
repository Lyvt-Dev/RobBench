#version 450
layout(location=0) in vec3 worldNormal;
layout(location=1) in vec3 vertexColor;
layout(push_constant) uniform Push {mat4 mvp;vec4 normalX;vec4 normalY;vec4 normalZ;vec4 color;} p;
layout(location=0) out vec4 outColor;
void main(){float light=.4+.6*max(dot(normalize(worldNormal),normalize(vec3(-.4,.8,-.6))),0);outColor=vec4(vertexColor*p.color.rgb*mix(1.0,light,p.color.a),1);}
