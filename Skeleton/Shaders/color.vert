#version 130
precision mediump float;

attribute vec3 vPositions; 
attribute vec3 vNormals;

uniform mat4 uMVP;
uniform mat4 modelmatrix;
uniform mat3 inv_modelmatrix;

varying vec3 vary_position;
varying vec3 vary_normal;

attribute vec2 vUV;
varying vec2 vary_UV;

void main()
{
	
	gl_Position =uMVP*vec4(vPositions, 1.0); 
	vary_normal = normalize(transpose(inv_modelmatrix)*vNormals);
	vec4 tmp = modelmatrix * vec4(vPositions, 1.0);
	tmp = tmp / tmp.w;
	vary_position=tmp.xyz;
	vary_UV = vUV;
}
