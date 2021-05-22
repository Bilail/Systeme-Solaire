#version 130
precision mediump float; //Medium precision for float. highp and smallp can also be used

varying vec3 vary_normal; //Sometimes we use "out" instead of "varying". "out" should be used in later version of GLSL.
varying vec3 vary_position;



uniform vec3 constants;
uniform float alpha ;
uniform vec3 lightcolor;
uniform vec3 lightposition;
uniform vec3 cameraposition;

varying vec2 vary_UV; //The UV coordinate, going from (0.0, 0.0) to (1.0, 1.0)
uniform sampler2D uTexture; //The texture



//We still use varying because OpenGLES 2.0 (OpenGL Embedded System, for example for smartphones) does not accept "in" and "out"

void main()
{
	vec3 color = texture(uTexture, vary_UV).rgb;
	float ka=constants[0];
	float kd=constants[1];
	float ks=constants[2];

	vec3 L = normalize(lightposition - vary_position);
	vec3 N = vary_normal;
	vec3 R = normalize(reflect(-L,N));
	vec3 V = normalize(cameraposition-vary_position);

	vec3 Ambiant = ka * color * lightcolor;

	vec3 Diffuse = kd*max(0.0,dot(N,L))*color*lightcolor;

	vec3 Specular = ks*pow(max(0.0,dot(R,V)),alpha)*lightcolor;
	
    gl_FragColor = vec4(Ambiant+Diffuse+Specular,1.0);
}
