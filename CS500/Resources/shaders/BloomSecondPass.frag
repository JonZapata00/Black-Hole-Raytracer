#version 430 core
out vec4 FragColor;
in vec2 TexCoords;

//rendered scene
uniform sampler2D scene;
//blurred scene by the bloom effect
uniform sampler2D blurredScene;
//Should we apply bloom?
uniform bool bloom;

//Applies tone mapping. I found this algorithm on the internet. Credits
//to rossning92
vec3 ToneMap(vec3 x) 
{
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{             
    const float gamma = 2.2;
    vec3 hdrColor = texture(scene, TexCoords).rgb;      
    vec3 bloomColor = texture(blurredScene, TexCoords).rgb;
    if(bloom) hdrColor += bloomColor; // additive blending
    //tone mapping
    vec3 result = ToneMap(hdrColor);
    //gamma correction
    result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
}