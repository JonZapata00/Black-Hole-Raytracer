#version 440 core
in vec3 TexCoords;
layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 brightColor;

//textures
uniform sampler2D diskTexture;
uniform sampler2D bbodyTexture;
uniform sampler2D noiseTexture;
uniform samplerCube cubeMap;

//camera/window related variables
uniform vec3 camPos;
uniform vec3 view;
uniform vec3 right;
uniform vec3 up;
uniform float aspectRatio;
uniform float focalLength;
uniform float halfWidth;
uniform float halfHeight;

//Black hole variables
uniform vec3 BHPos;
uniform float EHRad;
uniform float innerDiskRad;
uniform float outerDiskRad;
uniform bool applyLensing;
uniform bool renderDisk;
uniform float diskMax = 4.0;
const float BHTemperature = 10000.0f;
const float falloffRate = 10.0f;
uniform float beamExponent = 2.0f;

//other
uniform float timeElapsed;
const int numOctaves = 4;
const float STEP_SIZE = 0.1f;
const float PI = 3.14159;

//Given a point in cartesian coordinates, converts it
//to spherical
vec3 CartesianToSpherical(vec3 p)
{
    float rho = sqrt((p.x * p.x) + (p.y * p.y) + (p.z * p.z));
    float theta = atan(p.z, p.x);
    float phi = asin(p.y / rho);
    return vec3(rho, theta, phi);
}

//Checks whether a ray with origin = pos, direction = dir intersects a plane with point = pt, n = normal
float IntersectionRayPlane(vec3 pos, vec3 dir, vec3 pt, vec3 normal)
{
  float num = dot(pos - pt, normal);
  float den = dot(dir, normal);
  
  if (den == 0.0f) return -1.0f;
  
  float t = -num / den;
  
  if (t < 0.0f) return -1.0f;
  
  return t;
}

//Checks whether the given ray intersects the accretion disk
float IntersectionRayAccretionDisk(vec3 pos, vec3 dir, out vec3 intersectionPoint)
{
    float t = IntersectionRayPlane(pos, dir, BHPos, vec3(0, 1, 0));
    //the ray (segment) must first intersect the plane on which the disk lies
    if(t >= 0.0f && t <= STEP_SIZE)
    {
        //take squares for cheaper computations
        float innerRadSq = innerDiskRad * innerDiskRad;
        float outerRadSq = outerDiskRad * outerDiskRad;

        //we will store the intersection point if there is collision
        intersectionPoint = pos + t * normalize(dir);

        //if the distance between the intersection point and the center
        //of the Black Hole is between both radii, then there is an intersection
		vec3 vec = intersectionPoint - BHPos;
		float distSq = dot(vec, vec);
		if (distSq >= innerRadSq && distSq <= outerRadSq)
            return t;
    }
    
    return -1.0f;
}

//Gets the accretion disk color (upon intersecting with it).
//The physics related stuff (aka beaming, shifting, etc.) were taken
//from sean holloway's project.
vec3 GetAccretionDiskColor(vec3 intersectionPoint)
{
   //we get the distance from the intersection point to the center of the Black Hole
   float dist = length(intersectionPoint - BHPos);
   //compute the angle (convert to polar coordinates, taking into account we are working on 
   //the xz plane)
   float angle = atan(intersectionPoint.z, intersectionPoint.x);
   vec2 uv;
   //compute u coordinate for disk texture. We add an offset to the angle so that it gives the impression 
   //of movement
   uv.x = (angle + timeElapsed) / (2 * PI);
   //v coordinate
   uv.y = (dist - innerDiskRad) / (outerDiskRad - innerDiskRad);
   //using this color gives a cool effect
   vec3 orange = vec3( 1.3f, 0.65f, 0.3f );
   //sample the disk texture
   vec3 diskTextColor =  texture(diskTexture, uv).rgb * orange;
   
   //we now need to work in spherical (aka Schwarzschild coordinates).
   vec3 spherical = CartesianToSpherical(intersectionPoint);
   //again, we offset to give the impression of movement
   spherical.y += timeElapsed;
   vec3 noiseColor = texture(noiseTexture, uv).rgb + vec3(2);
   //take spherical coords
   float r = spherical.x;
   //remove the offset to properly sample
   float phi = spherical.z;

    // Reduce intensity over distance
    float falloff = max(1.0f - uv.y, 0.0f);
    noiseColor *= falloff;

    // Calculate temperature
    //We take the relation of T proportional to r^(-3/4)
    float rFactor = pow((3.0f * EHRad / r), 0.75f);
    float T = BHTemperature * rFactor;

    // Calculate doppler shift.
    float v = sqrt(EHRad / (2.0f * r));
    float gamma = 1.0f / sqrt(1.0f - (v * v));
    float incidence = spherical.z * r / length(spherical * vec3(1.0f, r, r));
    float shift = gamma * (1.0f + v * incidence);
    // Relativistic beaming
    vec3 beamColor = noiseColor * pow(abs(shift), beamExponent);
    shift *= sqrt(1 - (EHRad / r));

    //Sample blackbody texture (it is not really a black body texture, but it produces a cool effect).
    uv.x = (shift - 0.5f) / 2.0f;
    uv.y = uv.x;
    vec3 bbColor = texture(bbodyTexture, uv).rgb;

    // Apply the relativistic beaming to the sampled texture
    vec3 outColor = beamColor.xxx * bbColor;

    // Weight by Stefan-Boltzmann curve
    outColor *= pow(abs(T / BHTemperature), 4.0f);

    //Combine color with the disk texture to create accretion disk
    return outColor * diskTextColor;
}

//Generates a ray given the camera.
void GenerateRay(out vec3 pos, out vec3 dir)
{
	vec2 NDC;
	NDC.x = gl_FragCoord.x - halfWidth;
	NDC.x /= halfWidth;
	NDC.y = -(gl_FragCoord.y - halfHeight);
	NDC.y /= halfHeight;

	//computing the pixel position in world using the camera:
	// C + f*view + NDCx * right / (2 * plane width = 1) + NDCy * up / (2 * a)
	vec3 pixelWorld = camPos + focalLength * view;
    pixelWorld += NDC.x * right / 2.0f + NDC.y * up / (2.0f * aspectRatio);

    pos = camPos;
    dir = -normalize(camPos - pixelWorld);
}

//Applies the Schwarzschild Geodesic as the "magic potential"
void SchwarzschildGeodesic(float h2, vec3 pos, vec3 dir, out vec3 dx, out vec3 dv) 
{
  vec3 rayToBH = pos - BHPos;
  float r2 = dot(rayToBH, rayToBH);
  float r5 = pow(r2, 2.5);
  vec3 acc = -1.5 * h2 * pos / r5 * 1.0;
  dx = dir;
  dv = acc;
}

// Performs Runge-Kutta 4th Order integration
void IntegrateRungeKutta4(float h2, inout vec3 pos, inout vec3 dir)
{
    // Calculate k-factors
    vec3 dx1, du1, dx2, du2, dx3, du3, dx4, du4;
   
    SchwarzschildGeodesic(h2, pos, dir, dx1, du1);
    SchwarzschildGeodesic(h2, pos + dx1 * (STEP_SIZE / 2.0), dir + du1 * (STEP_SIZE / 2.0), dx2, du2);
    SchwarzschildGeodesic(h2, pos + dx2 * (STEP_SIZE / 2.0), dir + du2 * (STEP_SIZE / 2.0), dx3, du3);
    SchwarzschildGeodesic(h2, pos + dx3 * STEP_SIZE,         dir + du3 * STEP_SIZE,         dx4, du4);

    // Calculate full update
    pos += (STEP_SIZE / 6.0) * (dx1 + 2.0 * dx2 + 2.0 * dx3 + dx4);
    
    //light is only bent if lensing is applied
    if (applyLensing)
        dir += (STEP_SIZE / 6.0) * (du1 + 2.0 * du2 + 2.0 * du3 + du4);
}

//The bulk of the algorithm. Performs ray marching and checks for intersections
//while the light gets bent
vec3 RayMarch(vec3 pos, vec3 dir) 
{
  vec3 color = vec3(0.0, 0.0, 0.0);

  // Initial values. This is the angular momentum of orbiting particles.
  //this can be computed just once because our formula works if we fix orbits
  //at the equatorial plane of the Black Hole, meaning this vector will always be the same.
  vec3 h = cross(pos, dir);
  float h2 = dot(h, h);
 
  vec3 dx = dir;
  for (int i = 0; i < 300; i++) 
  {
      vec3 intersectionPoint;
      //Check intersection with disk
      if(renderDisk && IntersectionRayAccretionDisk(pos, dir, intersectionPoint) >= 0.0f)
           color += GetAccretionDiskColor(intersectionPoint);

      vec3 rayToBH = pos - BHPos;
      // Reach event horizon?
      if (dot(rayToBH, rayToBH) <= EHRad * EHRad) 
        return color;

       //integrate position and direction
      IntegrateRungeKutta4(h2, pos, dir);
  }

  //Finally, add skybox color. We need to sample it at the final ray direction
  color += texture(cubeMap, dir).rgb;
  return color;
}

///Main function
void main()
{
   vec3 pos;
   vec3 dir;
   GenerateRay(pos, dir);

   fragColor = vec4(RayMarch(pos, dir), 1.0);

   //Brightness computation for Bloom effect
   vec3 brightnessThreshold = vec3(0.2126, 0.5152, 0.02722);
   float brightness = dot(vec3(fragColor), brightnessThreshold);
    if(brightness > 1.0)
        brightColor = fragColor;
    else
        brightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
