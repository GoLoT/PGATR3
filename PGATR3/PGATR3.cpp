#include "pch.h"
#include <auxiliar.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <vector>

#define NUM_PARTICLES     (1024*1024)
#define WORK_GROUP_SIZE   128
#define SEED              123132414
#define XMIN              -10
#define XMAX              10
#define YMIN              -10
#define YMAX              10
#define ZMIN              -10
#define ZMAX              10
#define VXMIN             -10
#define VXMAX             10
#define VYMIN             -10
#define VYMAX             10
#define VZMIN             -10
#define VZMAX             10

const double M_PI = atan(1) * 4;
const double M_2PI = 2 * M_PI;

int CurrentWidth = 800,
CurrentHeight = 600,
WindowHandle = 0;
GLenum err;

//Functions
void Initialize(int, char*[]);
void InitWindow(int, char*[]);
void ResizeFunction(int, int);
void RenderFunction(void);
void InitSSBO();
void InitComputeShader();
void InitRenderingShader();
GLuint loadShader(const char *fileName, GLenum type);
void keyboardFunc(unsigned char key, int x, int y);
void idleFunc();
void UpdateCamera();
void mouseMotionFunc(int x, int y);
void mouseFunc(int button, int state, int x, int y);
void ResetSimulation();
void SortParticles();
void InitSortingShader();
void InitProfiler();
void ProfilerRender();
void ProfilerSort();
void ProfilerParticles();
const long long CurrentTimeMicroseconds();
void PrintProfilerStats();
bool TestOrder();

//Camera settings

struct CameraSettings
{
  long long lastT = 0;
  const float cameraSpeed = 50.0f;
  const float cameraSens = 2.0f / 100.0f;
  glm::vec3 cameraPos = { 0,0, -60.0f };
  float yaw = 0, pitch = 0;
  glm::mat4	proj = glm::mat4(1.0f);
  glm::mat4	view = glm::mat4(1.0f);
  int lastX = 0, lastY = 0;
  bool turning = false;
} cameraSettings;


typedef struct pos
{
  float x, y, z, w;
} pos;

typedef struct vel
{
  float vx, vy, vz, vw;
} vel;

typedef struct color
{
  float r, g, b, a;
} color;

struct ParticleShading
{
  int inPosition, inVelocity, inColor;
  int uProj, uView, uParticleRadius;
  GLuint program;
  GLuint vao;
  float particleRadius = 0.3f;
} particleShading;

struct ParticleCompute
{
  GLuint positionSSBO, velocitySSBO, colorSSBO;
  int inPosition = 4, inVelocity = 5, inColor = 6;
  GLuint program;
  bool paused = true;
  bool reset = false;
} particleCompute;

struct ParticleSort
{
  GLuint distanceSSBO, indexSSBO;
  int inPosition = 4, inDistances = 7, inIndices = 8;
  int uView, uJ, uK;
  GLuint preProgram;
  GLuint program;
  bool enabled = true;
} particleSort;

struct FrameProfiling
{
  unsigned int particlesTimes[50];
  unsigned int sortTimes[50];
  unsigned int renderTimes[50];
  unsigned int offset;
  unsigned long int lastTime;
} frameProfiling;

inline float ranf(const float min, const float max)
{
  return (float) ( (max - min)*(double)rand() / (double)(RAND_MAX) + min );
}

int main(int argc, char* argv[])
{
  Initialize(argc, argv);
  srand(SEED);
  InitComputeShader();
  InitSSBO();
  InitRenderingShader();
  InitSortingShader();
  InitProfiler();
  int maxWorkItems;
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxWorkItems);
	if (maxWorkItems < WORK_GROUP_SIZE)
	{
		std::cerr << "Error max number of work items are " << maxWorkItems << " and set " << WORK_GROUP_SIZE;
		exit(1);
	}

  glutMainLoop();

  exit(EXIT_SUCCESS);
}

void Initialize(int argc, char* argv[])
{
  InitWindow(argc, argv);

  fprintf(
    stdout,
    "INFO: OpenGL Version: %s\n",
    glGetString(GL_VERSION)
  );

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glEnable(GL_DEPTH_TEST);
  glFrontFace(GL_CW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_CULL_FACE);
  ResizeFunction(CurrentWidth, CurrentHeight);
  cameraSettings.view = glm::mat4(1.0f);
  cameraSettings.view[3].z = -60;
}

void InitWindow(int argc, char* argv[])
{
  glutInit(&argc, argv);

  glutInitContextVersion(4, 3);
  glutInitContextFlags(GLUT_FORWARD_COMPATIBLE);
  glutInitContextProfile(GLUT_CORE_PROFILE);

  glutSetOption(
    GLUT_ACTION_ON_WINDOW_CLOSE,
    GLUT_ACTION_GLUTMAINLOOP_RETURNS
  );

  glutInitWindowSize(CurrentWidth, CurrentHeight);

  glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);

  WindowHandle = glutCreateWindow("PGATR3 - Particles");

  if (WindowHandle < 1) {
    fprintf(
      stderr,
      "ERROR: Could not create a new rendering window.\n"
    );
    exit(EXIT_FAILURE);
  }

  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (GLEW_OK != err)
  {
    std::cout << "Error: " << glewGetErrorString(err) << std::endl;
    exit(-1);
  }
  const GLubyte *oglVersion = glGetString(GL_VERSION);
  std::cout << "This system supports OpenGL Version: " << oglVersion << std::endl;

  glutReshapeFunc(ResizeFunction);
  glutDisplayFunc(RenderFunction);
  glutKeyboardFunc(keyboardFunc);
  glutIdleFunc(idleFunc);
  glutMouseFunc(mouseFunc);
  glutMotionFunc(mouseMotionFunc);
}

void InitComputeShader()
{
  auto cshader = loadShader("../shaders/particles.comp", GL_COMPUTE_SHADER);
  particleCompute.program = glCreateProgram();
  glAttachShader(particleCompute.program, cshader);

  glLinkProgram(particleCompute.program);
  
  int linked;
  glGetProgramiv(particleCompute.program, GL_LINK_STATUS, &linked);
  if (!linked)
  {
    //Calculamos una cadena de error
    GLint logLen;
    glGetProgramiv(particleCompute.program, GL_INFO_LOG_LENGTH, &logLen);

    char *logString = new char[logLen];
    glGetProgramInfoLog(particleCompute.program, logLen, NULL, logString);
    std::cout << "Error: " << logString << std::endl;
    delete logString;

    glDeleteProgram(particleCompute.program);
    particleCompute.program = 0;
    exit(-1);
  }

  /*particleCompute.inPosition = glGetProgramResourceIndex(particleCompute.program, GL_SHADER_STORAGE_BLOCK, "Pos");
  particleCompute.inVelocity = glGetProgramResourceIndex(particleCompute.program, GL_SHADER_STORAGE_BLOCK, "Vel");
  particleCompute.inColor = glGetProgramResourceIndex(particleCompute.program, GL_SHADER_STORAGE_BLOCK, "Col");*/

}

void InitRenderingShader()
{
  auto vshader = loadShader("../shaders/particles.vert", GL_VERTEX_SHADER);
  auto fshader = loadShader("../shaders/particles.frag", GL_FRAGMENT_SHADER);
  auto gshader = loadShader("../shaders/particles.geom", GL_GEOMETRY_SHADER);
  particleShading.program = glCreateProgram();
  glAttachShader(particleShading.program, vshader);
  glAttachShader(particleShading.program, fshader);
  glAttachShader(particleShading.program, gshader);

  glLinkProgram(particleShading.program);

  int linked;
  glGetProgramiv(particleShading.program, GL_LINK_STATUS, &linked);
  if (!linked)
  {
    //Calculamos una cadena de error
    GLint logLen;
    glGetProgramiv(particleShading.program, GL_INFO_LOG_LENGTH, &logLen);

    char *logString = new char[logLen];
    glGetProgramInfoLog(particleShading.program, logLen, NULL, logString);
    std::cout << "Error: " << logString << std::endl;
    delete logString;

    glDeleteProgram(particleShading.program);
    particleShading.program = 0;
    exit(-1);
  }

  particleShading.inPosition = glGetAttribLocation(particleShading.program, "position");
  particleShading.inVelocity = glGetAttribLocation(particleShading.program, "velocity");
  particleShading.inColor = glGetAttribLocation(particleShading.program, "color");

  particleShading.uView = glGetUniformLocation(particleShading.program, "view");
  particleShading.uProj = glGetUniformLocation(particleShading.program, "proj");
  particleShading.uParticleRadius = glGetUniformLocation(particleShading.program, "particleRadius");

  glGenVertexArrays(1, &particleShading.vao);
}

void InitSortingShader()
{
  auto cshader = loadShader("../shaders/preSort.comp", GL_COMPUTE_SHADER);
  particleSort.preProgram = glCreateProgram();
  glAttachShader(particleSort.preProgram, cshader);

  glLinkProgram(particleSort.preProgram);

  int linked;
  glGetProgramiv(particleSort.preProgram, GL_LINK_STATUS, &linked);
  if (!linked)
  {
    //Calculamos una cadena de error
    GLint logLen;
    glGetProgramiv(particleSort.preProgram, GL_INFO_LOG_LENGTH, &logLen);

    char *logString = new char[logLen];
    glGetProgramInfoLog(particleSort.preProgram, logLen, NULL, logString);
    std::cout << "Error: " << logString << std::endl;
    delete logString;

    glDeleteProgram(particleSort.preProgram);
    particleSort.preProgram = 0;
    exit(-1);
  }

  particleSort.uView = glGetUniformLocation(particleSort.preProgram, "view");

  cshader = loadShader("../shaders/particleSort.comp", GL_COMPUTE_SHADER);
  particleSort.program = glCreateProgram();
  glAttachShader(particleSort.program, cshader);

  glLinkProgram(particleSort.program);

  glGetProgramiv(particleSort.program, GL_LINK_STATUS, &linked);
  if (!linked)
  {
    //Calculamos una cadena de error
    GLint logLen;
    glGetProgramiv(particleSort.program, GL_INFO_LOG_LENGTH, &logLen);

    char *logString = new char[logLen];
    glGetProgramInfoLog(particleSort.program, logLen, NULL, logString);
    std::cout << "Error: " << logString << std::endl;
    delete logString;

    glDeleteProgram(particleSort.program);
    particleSort.program = 0;
    exit(-1);
  }

  particleSort.uJ = glGetUniformLocation(particleSort.program, "j");
  particleSort.uK = glGetUniformLocation(particleSort.program, "k");

  glGenBuffers(1, &particleSort.distanceSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSort.distanceSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &particleSort.indexSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSort.indexSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);

  GLuint* indices = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, NUM_PARTICLES * sizeof(GLuint), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
  for (int i = 0; i < NUM_PARTICLES; i++)
  {
    indices[i] = i;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

}


void InitSSBO()
{
  glGenBuffers(1, &particleCompute.positionSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleCompute.positionSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(pos), NULL, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &particleCompute.velocitySSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleCompute.velocitySSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(vel), NULL, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &particleCompute.colorSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleCompute.colorSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(color), NULL, GL_DYNAMIC_DRAW);

  ResetSimulation();
}

void ResetSimulation()
{
  GLint bufMask = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleCompute.positionSSBO);
  pos* points = (pos*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, NUM_PARTICLES * sizeof(pos), bufMask);
  for (int i = 0; i < NUM_PARTICLES; i++)
  {
    points[i].x = ranf(XMIN, XMAX);
    points[i].y = ranf(YMIN, YMAX);
    points[i].z = ranf(ZMIN, ZMAX);
    points[i].w = 1;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleCompute.velocitySSBO);
  vel* vels = (vel*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, NUM_PARTICLES * sizeof(vel), bufMask);
  for (int i = 0; i < NUM_PARTICLES; i++)
  {
    vels[i].vx = ranf(VXMIN, VXMAX);
    vels[i].vy = ranf(VYMIN, VYMAX);
    vels[i].vz = ranf(VZMIN, VZMAX);
    vels[i].vw = 0;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleCompute.colorSSBO);
  color* colors = (color*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, NUM_PARTICLES * sizeof(color), bufMask);
  for (int i = 0; i < NUM_PARTICLES; i++)
  {
    colors[i].r = ranf(.3f, 1.);
    colors[i].g = ranf(.3f, 1.);
    colors[i].b = ranf(.3f, 1.);
    colors[i].a = 1.;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


void ResizeFunction(int Width, int Height)
{
  CurrentWidth = Width;
  CurrentHeight = Height;
  glViewport(0, 0, CurrentWidth, CurrentHeight);
  cameraSettings.proj = glm::perspective(glm::radians(60.0f), (float)CurrentWidth/(float)CurrentHeight, 0.1f, 2000.0f);
}

void RenderFunction(void)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  UpdateCamera();

  if (particleCompute.reset) {
    particleCompute.reset = false;
    ResetSimulation();
  }

  if (!particleCompute.paused) {
	  glUseProgram(particleCompute.program);

	  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleCompute.inPosition, particleCompute.positionSSBO);
	  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleCompute.inVelocity, particleCompute.velocitySSBO);
	  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleCompute.inColor, particleCompute.colorSSBO);

	  glDispatchCompute(NUM_PARTICLES / WORK_GROUP_SIZE, 1, 1);
	  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  }
  ProfilerParticles();
  if (particleSort.enabled) {
    SortParticles();
  }
  ProfilerSort();

  glUseProgram(particleShading.program);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (particleShading.uView != -1)
    glUniformMatrix4fv(particleShading.uView, 1, GL_FALSE, &(cameraSettings.view[0][0]));
  if (particleShading.uProj != -1)
    glUniformMatrix4fv(particleShading.uProj, 1, GL_FALSE, &(cameraSettings.proj[0][0]));
  if (particleShading.uParticleRadius != -1)
    glUniform1f(particleShading.uParticleRadius, particleShading.particleRadius);
  
  glBindVertexArray(particleShading.vao);

  if (particleShading.inPosition != -1) {
    glEnableVertexAttribArray(particleShading.inPosition);
    glBindBuffer(GL_ARRAY_BUFFER, particleCompute.positionSSBO);
    glVertexAttribPointer(particleShading.inPosition, 4, GL_FLOAT, GL_FALSE, 0, 0);
  }
  if (particleShading.inVelocity != -1) {
    glEnableVertexAttribArray(particleShading.inVelocity);
    glBindBuffer(GL_ARRAY_BUFFER, particleCompute.velocitySSBO);
    glVertexAttribPointer(particleShading.inVelocity, 4, GL_FLOAT, GL_FALSE, 0, 0);
  }
  if (particleShading.inColor != -1) {
    glEnableVertexAttribArray(particleShading.inColor);
    glBindBuffer(GL_ARRAY_BUFFER, particleCompute.colorSSBO);
    glVertexAttribPointer(particleShading.inColor, 4, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if(particleSort.enabled)
  {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, particleSort.indexSSBO);
    glDrawElements(GL_POINTS, NUM_PARTICLES, GL_UNSIGNED_INT, 0);
  } else 
    glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDisable(GL_BLEND);

  glutSwapBuffers();

  ProfilerRender();
}

void SortParticles()
{
  glUseProgram(particleSort.preProgram);

  /*if (particleSort.uCamPos != -1)
    glUniform3f(particleSort.uCamPos, cameraSettings.cameraPos[0], cameraSettings.cameraPos[1], cameraSettings.cameraPos[2]);*/

  if (particleSort.uView != -1)
    glUniformMatrix4fv(particleSort.uView, 1, GL_FALSE, &(cameraSettings.view[0][0]));

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleSort.inPosition, particleCompute.positionSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleSort.inDistances, particleSort.distanceSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleSort.inIndices, particleSort.indexSSBO);

  glDispatchCompute(NUM_PARTICLES / WORK_GROUP_SIZE, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	
  /*glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSort.distanceSSBO);
  GLfloat* distances = (GLfloat*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
  for (int i = 0; i < 5; i++)
  {
    std::cout << i << " " << distances[i] << std::endl;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);*/ 


  /*glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSort.indexSSBO);
  GLuint* indices = (GLuint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
  for (int i = 0; i < 5; i++)
  {
	  std::cout << i << " " << indices[i] << std::endl;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);*/

  /*size_t data_size = NUM_PARTICLES / 2;
  size_t local_size = WORK_GROUP_SIZE;
  size_t num_workgroups = data_size / local_size;*/
  
  glUseProgram(particleSort.program);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleSort.inDistances, particleSort.distanceSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particleSort.inIndices, particleSort.indexSSBO);

	int j, k;
  /* Major step */
  for (k = 2; k <= NUM_PARTICLES; k <<= 1) {
	      glUniform1ui(particleSort.uK, k);

    /* Minor step */
    for (j=k>>1; j>0; j=j>>1) {
		      glUniform1ui(particleSort.uJ, j);

      glDispatchCompute(NUM_PARTICLES/WORK_GROUP_SIZE,1,1);
	  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}
}

  //std::cout << "Test de ordenacion: " << (TestOrder()?"Pasado":"Fallado") << std::endl;


  /*glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSort.indexSSBO);
  GLuint* indices2 = (GLuint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
  for (int i = 0; i < 5; i++)
  {
    std::cout << i << " " << indices2[NUM_PARTICLES-i] << std::endl;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);*/

}

//Shader loading code from G3D exercises
GLuint loadShader(const char *fileName, GLenum type) {
  unsigned int fileLen;
  char *source = loadStringFromFile(fileName, fileLen);
  //////////////////////////////////////////////
  //Creación y compilación del Shader
  GLuint shader;
  shader = glCreateShader(type);
  glShaderSource(shader, 1,
    (const GLchar **)&source, (const GLint *)&fileLen);
  glCompileShader(shader);
  delete[] source;

  //Comprobamos que se compiló bien
  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled)
  {
    //Calculamos una cadena de error
    GLint logLen;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    char *logString = new char[logLen];
    glGetShaderInfoLog(shader, logLen, NULL, logString);
    std::cout << "Error: " << logString << std::endl;
    delete[] logString;
    glDeleteShader(shader);
    exit(-1);
  }

  return shader;
}

void keyboardFunc(unsigned char key, int x, int y)
{
  const long long t = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
    ).count();
  double delta = 0;
  delta = (t - cameraSettings.lastT) / 1000.0f;
  cameraSettings.lastT = t;
  if (delta > 0.016)
    delta = 0.016;

  glm::vec3 forward = glm::vec3(cameraSettings.view[0][2], cameraSettings.view[1][2], cameraSettings.view[2][2]);
  if (forward.z != 0 && forward.x != 0) {
    forward.y = 0;
    forward = glm::normalize(forward);
  }
  glm::vec3 right = glm::vec3(cameraSettings.view[0][0], cameraSettings.view[1][0], cameraSettings.view[2][0]);
  glm::vec3 up = glm::vec3(0, -1.0f, 0);

  switch (key)
  {
  case 'w':
  case 'W':
    forward *= delta * cameraSettings.cameraSpeed;
    cameraSettings.cameraPos += forward;
    break;
  case 's':
  case 'S':
    forward *= -delta * cameraSettings.cameraSpeed;
    cameraSettings.cameraPos += forward;
    break;
  case 'a':
  case 'A':
    right *= delta * cameraSettings.cameraSpeed;
    cameraSettings.cameraPos += right;
    break;
  case 'd':
  case 'D':
    right *= -delta * cameraSettings.cameraSpeed;
    cameraSettings.cameraPos += right;
    break;
  case 'e':
  case 'E':
    cameraSettings.yaw += (float)delta * cameraSettings.cameraSpeed / 2;
    if (cameraSettings.yaw > M_2PI)
      cameraSettings.yaw -= (float)M_2PI;
    break;
  case 'q':
  case 'Q':
    cameraSettings.yaw -= (float)delta * cameraSettings.cameraSpeed / 2;
    if (cameraSettings.yaw < 0)
      cameraSettings.yaw += (float)M_2PI;
    break;
  case ' ':
    up *= delta * cameraSettings.cameraSpeed;
    cameraSettings.cameraPos += up;
    break;
  case 'z':
  case 'Z':
    up *= -delta * cameraSettings.cameraSpeed;
    cameraSettings.cameraPos += up;
    break;
  case 'p':
  case 'P':
	  particleCompute.paused = !particleCompute.paused;
	  break;
  case 'r':
  case 'R':
    particleCompute.reset = true;
    break;
  case 't':
  case 'T':
    particleSort.enabled = !particleSort.enabled;
    break;
  case 'l':
  case 'L':
    PrintProfilerStats();
    break;
  case '+':
    particleShading.particleRadius += 0.05f;
    if (particleShading.particleRadius > 2.0f) particleShading.particleRadius = 2.0f;
    break;
  case '-':
    particleShading.particleRadius -= 0.05f;
    if (particleShading.particleRadius < 0.05f) particleShading.particleRadius = 0.05f;
    break;
  default:
    std::cout << "Se ha pulsado la tecla " << key << std::endl << std::endl;
  }
  glutPostRedisplay();
}

void idleFunc() {
  /*const long long t = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
    ).count();*/

  glutPostRedisplay();
}

void UpdateCamera()
{
  cameraSettings.view = glm::mat4(1.0f);
  cameraSettings.view = glm::rotate(cameraSettings.view, cameraSettings.pitch, glm::vec3(1.0f, 0, 0));
  cameraSettings.view = glm::rotate(cameraSettings.view, cameraSettings.yaw, glm::vec3(0, 1.0f, 0));
  cameraSettings.view = glm::translate(cameraSettings.view, cameraSettings.cameraPos);
}

void mouseFunc(int button, int state, int x, int y)
{
  if (button == 0) {
    cameraSettings.lastX = x;
    cameraSettings.lastY = y;
    cameraSettings.turning = !state;
  }
}

void mouseMotionFunc(int x, int y)
{
  if (!cameraSettings.turning)
    return;
  int delta = x - cameraSettings.lastX;
  cameraSettings.yaw += delta * cameraSettings.cameraSens;
  if (cameraSettings.yaw > M_2PI)
    cameraSettings.yaw -= (float)M_2PI;
  if (cameraSettings.yaw < 0)
    cameraSettings.yaw += (float)M_2PI;

  delta = y - cameraSettings.lastY;
  cameraSettings.pitch += delta * cameraSettings.cameraSens;
  //Clamp a -170º, 170º
  cameraSettings.pitch = glm::clamp(cameraSettings.pitch, -1.5708f, 1.5708f);

  cameraSettings.lastX = x; cameraSettings.lastY = y;
}

void InitProfiler()
{
	for (int i = 0; i < 50; i++) {
		frameProfiling.renderTimes[i] = 0;
		frameProfiling.particlesTimes[i] = 0;
		frameProfiling.sortTimes[i] = 0;
	}
  frameProfiling.lastTime = (unsigned long) CurrentTimeMicroseconds();
}

void ProfilerRender()
{
  unsigned long int oldTime = frameProfiling.lastTime;
  frameProfiling.lastTime = (unsigned long) CurrentTimeMicroseconds();
  frameProfiling.renderTimes[frameProfiling.offset++] = frameProfiling.lastTime - oldTime;
  if (frameProfiling.offset >= 50)
    frameProfiling.offset = 0;
}

void ProfilerParticles()
{
	unsigned long int oldTime = frameProfiling.lastTime;
	frameProfiling.lastTime = (unsigned long)CurrentTimeMicroseconds();
	frameProfiling.particlesTimes[frameProfiling.offset] = frameProfiling.lastTime - oldTime;
}

void ProfilerSort()
{
	unsigned long int oldTime = frameProfiling.lastTime;
	frameProfiling.lastTime = (unsigned long)CurrentTimeMicroseconds();
	frameProfiling.sortTimes[frameProfiling.offset] = frameProfiling.lastTime - oldTime;
}

void PrintProfilerStats()
{
  int i;
  unsigned long int sumRender = 0;
  unsigned long int sumParticles = 0;
  unsigned long int sumSort = 0;
  for (i = 0; i < 50; i++)
  {
    sumRender += frameProfiling.renderTimes[i];
	sumParticles += frameProfiling.particlesTimes[i];
	sumSort += frameProfiling.sortTimes[i];
  }
  float meanRender = ((float)sumRender/ (float)i) / 1000.0f;
  float meanParticles = ((float)sumParticles / (float)i) / 1000.0f;
  float meanSort = ((float)sumSort / (float)i) / 1000.0f;
  float totalFrameTime = meanRender + meanParticles + meanSort;
  std::cout << "Media de frametime: " << totalFrameTime << std::endl << "Media de framerate: " << 1000.0f / totalFrameTime << std::endl;
  std::cout << "\tMedia de particulas: " << meanParticles << std::endl;
  std::cout << "\tMedia de ordenamiento: " << meanSort << std::endl;
  std::cout << "\tMedia de render: " << meanRender << std::endl;
}

inline const long long CurrentTimeMicroseconds()
{
  return std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool TestOrder()
{
	GLfloat* distancesH = (GLfloat*) malloc(NUM_PARTICLES * sizeof(GLfloat));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSort.distanceSSBO);
	GLfloat* distances = (GLfloat*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
	memcpy(distancesH, distances, NUM_PARTICLES * sizeof(GLfloat));
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	GLuint* indicesH = (GLuint*)malloc(NUM_PARTICLES * sizeof(GLuint));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSort.indexSSBO);
	GLuint* indices = (GLuint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
	memcpy(indicesH, indices, NUM_PARTICLES * sizeof(GLuint));
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  GLfloat last = distancesH[indicesH[0]];
  for (int i = 1; i < NUM_PARTICLES; i++)
  {
    GLfloat act = distancesH[indicesH[i]];
    if (act > last)
    {
      std::cout << "Distancia incorrecta en " << i << " -> " << indicesH[i] << " Distancia: " << act << " < " << last << std::endl;
      free(distancesH);
      free(indicesH);
      return false;
    }
    last = act;
  }

  free(distancesH);
  free(indicesH);
	return true;

}