#version 430 core

//Input data from vertex shader
in vec2 texCoord;
in vec3 Position_worldspace;
in vec3 Normal_cameraspace;
in vec3 EyeDirection_cameraspace;

//Ouput data
out vec3 colour;

//Data structure to match input from shader storage buffer object
struct RawLightSource {
  vec4 geometry;
  vec4 colour;
  vec4 diffuse;
  vec4 specular;
  vec4 power;
};

//Cleaned up data structure
struct LightSource {
  vec3 geometry;
  vec3 colour;
  vec3 diffuse;
  vec3 specular;
  float power;
};

//Lighting inputs from shader storage buffer
layout(std430, binding = 0) buffer LightPropertiesBuffer {
  RawLightSource lightSources[];
};

//Engine inputs
uniform sampler2D textureSampler;
uniform mat4 V;
uniform vec3 ambientLight;

vec3 calcDiffuseLight(LightSource lightSource, vec3 materialColour, vec3 lightDirection, vec3 normal) {
  //Cosine of the angle between the normal and light direction
  float cosTheta = clamp(dot(normal, lightDirection), 0, 1);

  return(lightSource.diffuse * materialColour * lightSource.colour * lightSource.power * cosTheta);
}

vec3 calcSpecularLight(LightSource lightSource, vec3 lightDirection, vec3 normal, vec3 eyeDirection) {
  //Only keep direction component
  eyeDirection = normalize(eyeDirection);

  //Direction of refledted light
  vec3 reflectionDirection = reflect(-lightDirection, normal);
  //Cosine of the angle between the eye and reflection vectors
  float cosAlpha = clamp(dot(eyeDirection, reflectionDirection), 0, 1);

  return(lightSource.specular * lightSource.colour * lightSource.power * pow(cosAlpha, 5));
}

vec3 calcPointLight(LightSource lightSource, vec3 materialColour, vec3 fragPos, vec3 normal, vec3 eyeDirection) {
  //Vector from vertex to light (camera space)
  vec3 LightPosition_cameraspace = (V * vec4(lightSource.geometry, 1)).xyz;
  vec3 LightDirection_cameraspace = LightPosition_cameraspace + eyeDirection;

  //Distance to the light
  float lightDistanceSqr = pow(distance(lightSource.geometry, fragPos), 2);

  //Direction of the light (from the fragment to the light)
  vec3 lightDirection = normalize(LightDirection_cameraspace);

  //Calculate lighting components
  vec3 diffuse = calcDiffuseLight(lightSource, materialColour, lightDirection, normal);
  vec3 specular = calcSpecularLight(lightSource, lightDirection, normal, eyeDirection);

  return((diffuse + specular) / lightDistanceSqr);
}

vec3 calcDirectionalLight(LightSource lightSource, vec3 materialColour, vec3 fragPos, vec3 normal, vec3 eyeDirection) {
  //Direction of the light (from the light to the fragment)
  vec3 lightDirection = normalize(-(V * vec4(lightSource.geometry, 0)).xyz);

  //Calculate lighting components
  vec3 diffuse = calcDiffuseLight(lightSource, materialColour, lightDirection, normal);
  vec3 specular = calcSpecularLight(lightSource, lightDirection, normal, eyeDirection);

  return(diffuse + specular);
}

void main() {
  //Base colour of the fragment
  vec3 materialColour = texture(textureSampler, texCoord).rgb;
  colour = vec3(0.0, 0.0, 0.0);

  //Normal of the fragment (camera space)
  vec3 normal = normalize(Normal_cameraspace);

  //Eye vector (towards the camera)
  vec3 eyeDirection = EyeDirection_cameraspace;

  //Calculate lighting influence from each light source
  LightSource lightSource;
  int lightCount = lightSources.length();
  for (int i = 0; i < lightCount; i++) {
    lightSource.geometry = lightSources[i].geometry.xyz;
    lightSource.colour = lightSources[i].colour.xyz;
    lightSource.diffuse = lightSources[i].diffuse.xyz;
    lightSource.specular = lightSources[i].specular.xyz;
    lightSource.power = lightSources[i].power.x;

    //Calculate fragment colour from current light
    colour += calcPointLight(lightSource, materialColour, Position_worldspace, normal, eyeDirection);
  }

  //Add ambient lighting
  colour += ambientLight * materialColour;
}