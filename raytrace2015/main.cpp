//
// template-rt.cpp
//

#define _CRT_SECURE_NO_WARNINGS
#include "matm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

// -------------------------------------------------------------------
// Ray struct

struct Ray
{
    vec3 origin;    // origin of the ray
    vec3 dir;     // direction of the ray
};


// -------------------------------------------------------------------
// Sphere struct

struct Sphere
{ 
  vec3 center;
  float radius;
  // ambient, diffuse and specular reflection constant
  //   in Phong's reflection model
  vec3 ka, kd, ks;
  // control how much light is received from reflection
  //   in recursive ray-tracing (e.g. 0.1)
  vec3 reflectivity;
  // control size of specular highlight (e.g. 20)
  float alpha;

  // default constructor
  Sphere(const vec3& ic=vec3(0.0f), const float& ir=0.0f,
         const vec3& ika=vec3(0.0f), const vec3& ikd=vec3(0.0f), const vec3& iks=vec3(0.0f),
         const float& ireflectivity=0.1f, const float& ialpha=1.0f) :
  center(ic), radius(ir),
  ka(ika), kd(ikd), ks(iks),
  reflectivity(ireflectivity), alpha(ialpha)
  {}

  bool intersect(const Ray& ray, float& t0, float& t1);
};

// return true if the input ray intersects with the sphere; otherwise return false
// return by reference the intersections. t0 refers to closer intersection, t1 refers to farther intersection
bool Sphere::intersect(const Ray& ray, float& t0, float& t1)
{
  vec3 rDir    = ray.dir;
  vec3 rOrigin = ray.origin;
  vec3 sOrigin = center;

  vec3 fromStoR = rOrigin - sOrigin;

  // QUADRATIC PARAMETERS
  float a = powf(length(rDir), 2.0f);
  float b = 2.0f * dot(rDir, fromStoR);
  float c = powf(length(fromStoR), 2.0f) - powf(radius, 2.0f);

  float sqrtTest = b * b - 4 * a * c;

  if (sqrtTest >= 0)
  {
    t0 = (-b - sqrt(sqrtTest)) / (2 * a);
    t1 = (-b + sqrt(sqrtTest)) / (2 * a);
    
    return (t0 > 0 && t1 > 0);
  }
  else
    return false;
};


// -------------------------------------------------------------------
// Light Structs

struct AmbientLight
{
  // ambient intensity (a vec3 of 0.0 to 1.0, each entry in vector refers to R,G,B channel)
  vec3 ia;

  // default constructor
  AmbientLight(const vec3& iia=vec3(0.0f)) : ia(iia) {}
};


struct PointLight
{
  vec3 location;  // location of point light
  vec3 id, is;  // diffuse intensity, specular intensity (vec3 of 0.0 to 1.0)
  
  // default constructor
  PointLight(const vec3& iloc=vec3(0.0f), const vec3& iid=vec3(0.0f),
             const vec3& iis=vec3(0.0f)) : location(iloc), id(iid), is(iis) {}
};


// -------------------------------------------------------------------
// Our Scene

// lights and spheres in our scene
AmbientLight my_ambient_light;      // our ambient light
vector<PointLight> my_point_lights;   // a vector containing all our point lights
vector<Sphere> my_spheres;        // a vector containing all our spheres


// this stores the color of each pixel, which will take the ray-tracing results
vector<vec3> g_colors;  

int recursion_lvl_max = 2;
int sample_lv = 1;

// this defines the screen
int g_width  =  640;        //number of pixels
int g_height =  480;       // "g_" refers to coord in 2D image space
float fov =  30;          // field of view (in degree)

float invWidth = 1 / float(g_width);
float invHeight = 1 / float(g_height);
float aspectratio = g_width / float(g_height);
float angle = tan(M_PI * 0.5 * fov / float(180));


// -------------------------------------------------------------------
// Utilities

void setColor(int ix, int iy, const vec3& color)
{
  int iy2 = g_height - iy - 1; // Invert iy coordinate.
  g_colors[iy2 * g_width + ix] = color;
}



// -------------------------------------------------------------------
// Ray tracing

vec3 trace(const Ray& ray, int recursion_lvl)
{
  float inf = 99999;
  float t_min = inf;
  int near_sphere_idx;
  bool has_intersection = false;

  for (int i=0; i<my_spheres.size(); ++i)
  {
    // distance from ray's direction to intersection (t0 then t1)
    float t0 = inf;   //some large value
    float t1 = inf;

    // check intersection with sphere
    if (my_spheres[i].intersect(ray, t0, t1))
    {
      has_intersection = true;

      if (t0<t_min)
      {
        t_min = t0;
        near_sphere_idx = i;
      }
    } 
  }

  if (has_intersection == false)
    // just return background color (black)
      return vec3(0.0f, 0.0f, 0.0f);

  Sphere my_sphere = my_spheres[near_sphere_idx];


  // PHONG MODEL
  vec3 color = vec3(0.0f, 0.0f, 0.0f);

  // #1: Ambient Light
  vec3 iAmbient = my_sphere.ka * my_ambient_light.ia;
  color += iAmbient;

  // #2+3: Diffuse + Specular Light
  // finding normal of surface at intersection and to viewer
  vec3 intersect = ray.origin + ray.dir * t_min;
  vec3 normal = normalize(intersect - my_sphere.center);
  vec3 toViewer = normalize(-intersect);

  for (int i = 0; i < my_point_lights.size(); ++i)
  {
    // finding direction from intersection to light, reflection
    vec3 lightDir = normalize(my_point_lights[i].location - intersect);

    // testing for occlusion
    Ray obsTest;
    obsTest.origin = intersect;
    obsTest.dir = lightDir;
    float ta, tb;
    bool isNotOccluded = true;

    for (int j = 0; j < my_spheres.size() && isNotOccluded; ++j)
      if (my_spheres[j].intersect(obsTest, ta, tb))
        isNotOccluded = false;

    if (isNotOccluded)
    {
      // SPECULAR & HALF-VECTOR DEFINITIN
      vec3 reflect  = normalize(normal * 2 * dot(lightDir, normal) - lightDir);
      //vec3 halfVect = normalize(lightDir + toViewer); 

      // get temporary diffuse and specular value
      float dDotTest = dot(normal, lightDir);
            dDotTest = (dDotTest > 0) ? dDotTest : 0;
      vec3 iDiffuse = my_sphere.kd * my_point_lights[i].id * dDotTest; 

      // NORMAL REFLECTION
      float sDotTest = dot(reflect, toViewer);
            sDotTest = (sDotTest > 0) ? sDotTest : 0;
      vec3 iSpecular = my_sphere.ks * my_point_lights[i].is * pow(sDotTest, my_sphere.alpha);
      // HALF-VECTOR REFLECTION
      /*
      float sDotTest = dot(normal, halfVect);
            sDotTest = (sDotTest > 0) ? sDotTest : 0;
      vec3 iSpecular = my_sphere.ks * my_point_lights[i].is * pow(sDotTest, my_sphere.alpha);
      */

      color += iDiffuse + iSpecular;
    }
  }


  if (recursion_lvl > 0)
  {
    Ray newRay;
    newRay.dir    = normalize(normal * -2 * dot(ray.dir, normal) + ray.dir);
    newRay.origin = intersect;

    return color + trace(newRay, recursion_lvl-1) * my_sphere.reflectivity;
  }
  else 
  {
    return color;
  }     
}


// returns the normalized direction from origin with given pixel location
vec3 getDir(float ix, float iy)
{
  vec3 dir;
  dir.x = (2 * (ix * invWidth) - 1) * angle * aspectratio;
  dir.y = (2 * (iy * invHeight) - 1) * angle;
  dir.z = -1;

  return dir;
}

// render the pixel by firing the ray there
void renderPixel(int ix, int iy)
{
  vec3 color = vec3(0,0,0);
  float div = 1.0f / sample_lv;
  float count = 1.0f / sample_lv / sample_lv;

  for (int i = 0; i < sample_lv; ++i)
  {
    for (int j = 0; j < sample_lv; ++j)
    {
      Ray ray;
      ray.origin = vec3(0.0f, 0.0f, 0.0f);
      ray.dir = getDir(ix + div * (0.5f * i), iy + div * (0.5f * j));
      color += trace(ray, recursion_lvl_max) * count;
    }
  }

  // limiting the value or RGB to the max if go past
  color.x = (color.x <= 1) ? color.x : 1;
  color.y = (color.y <= 1) ? color.y : 1;
  color.z = (color.z <= 1) ? color.z : 1;

  setColor(ix, iy, color);
}

// render the full image
void render()
{
  for (int iy = 0; iy < g_height; iy++)
    for (int ix = 0; ix < g_width; ix++)
      renderPixel(ix, iy);
}


// -------------------------------------------------------------------
// PPM saving

void savePPM(int Width, int Height, char* fname, unsigned char* pixels) 
{
  FILE *fp;
  const int maxVal=255;
    
  printf("Saving image %s: %d x %d\n", fname, Width, Height);
  fp = fopen(fname,"wb");
  if (!fp)
  {
    printf("Unable to open file '%s'\n", fname);
    return;
  }
  fprintf(fp, "P6\n");
  fprintf(fp, "%d %d\n", Width, Height);
  fprintf(fp, "%d\n", maxVal);

  for(int j = 0; j < Height; j++) {
    fwrite(&pixels[j*Width*3], 3, Width, fp);
  }
    
  fclose(fp);
}

void saveFile()
{
  // Convert color components from floats to unsigned chars.
  // clamp values if out of range.
  unsigned char* buf = new unsigned char[g_width * g_height * 3];
  for (int y = 0; y < g_height; y++)
    for (int x = 0; x < g_width; x++)
      for (int i = 0; i < 3; i++)
        buf[y*g_width*3+x*3+i] = (unsigned char)(((float*)g_colors[y*g_width+x])[i] * 255.9f);
    
  // change file name based on input file name.
  savePPM(g_width, g_height, "output.ppm", buf);
  delete[] buf;
}


// -------------------------------------------------------------------
// Main

/* MODIFIED MAIN SETUP */
/*
int main(int argc, char* argv[])
{
  // setup pixel array
  g_colors.resize(g_width * g_height);

  // setup our scene...

  // setup ambient light
  my_ambient_light = AmbientLight(vec3(0.1));

  // setup point lights
  my_point_lights.push_back(
    PointLight(vec3(3,3,0),
               vec3(0.5, 0.5, 0.5), vec3(0.5,0.5,0.5))
  );

  my_point_lights.push_back(
    PointLight(vec3(-3,-3,0),
               vec3(0.1, 0.1, 0.1), vec3(0.1,0.1,0.1))
  );

  // setup spheres
  my_spheres.push_back(  // black
    Sphere(vec3(1.5,-1,-10), 1.0,
           vec3(0.1,0.1,0.1), vec3(0.5,0.5,0.5), vec3(0.2,0.2,0.2),
           0.5, 100.0) );

  my_spheres.push_back(
    Sphere(vec3(-1.5,0.5,-8), 0.5,
           vec3(0.0,1.0,0.0), vec3(0.0,1.0,0.0), vec3(0.5,0.5,0.5),
           0.0, 10.0) );

  my_spheres.push_back(
    Sphere(vec3(0,0,-30), 15,
           vec3(1.0,0.0,0.0), vec3(1.0,0.0,0.0), vec3(1.0,1.0,1.0),
           0.0, 10.0) );

  render();
  saveFile();

  return 0;
}
*/

/* ORIGINAL MAIN */
int main(int argc, char* argv[])
{
  // setup pixel array
  g_colors.resize(g_width * g_height);

  // setup our scene...

  // setup ambient light
  my_ambient_light = AmbientLight(vec3(0.1));

  // setup point lights
  my_point_lights.push_back(
    PointLight(vec3(3,3,0),
               vec3(0.5, 0.5, 0.5), vec3(0.5,0.5,0.5))
  );

  my_point_lights.push_back(
    PointLight(vec3(-3,-3,0),
               vec3(0.1, 0.1, 0.1), vec3(0.1,0.1,0.1))
  );

  // setup spheres
  my_spheres.push_back(  // black
    Sphere(vec3(0,0,-10), 1.0,
           vec3(0.1,0.1,0.1), vec3(0.5,0.5,0.5), vec3(0.2,0.2,0.2),
           0.5, 100.0) );

  my_spheres.push_back(
    Sphere(vec3(-1.5,0.5,-8), 0.5,
           vec3(0.0,1.0,0.0), vec3(0.0,1.0,0.0), vec3(0.5,0.5,0.5),
           0.0, 10.0) );

  render();
  saveFile();

  return 0;
}
