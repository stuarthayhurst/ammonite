#include <iostream>
#include <cstdlib>
#include <string>

#include <random>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "ammonite/ammonite.hpp"
#include "common/argHandler.hpp"

void printMetrics(double frameTime) {
  double frameRate = 0.0;
  if (frameTime != 0.0) {
    frameRate = 1 / frameTime;
  }

  std::printf("%.2f fps", frameRate);
  std::printf(" (%fms)\n", frameTime * 1000);
}

void cleanUp(int modelCount, int loadedModelIds[]) {
  //Cleanup
  for (int i = 0; i < modelCount; i++) {
    ammonite::models::deleteModel(loadedModelIds[i]);
  }
  ammonite::shaders::eraseShaders();
  ammonite::windowManager::setup::destroyGlfw();
}

int main(int argc, char* argv[]) {
  //Handle arguments
  const int showHelp = arguments::searchArgument(argc, argv, "--help", true, nullptr);
  if (showHelp == 1) {
    std::cout << "Program help: \n"
    " --help:       Display this help page\n"
    " --benchmark:  Start a benchmark\n"
    " --vsync:      Enable / disable VSync (true / false)" << std::endl;
    return EXIT_SUCCESS;
  } else if (showHelp == -1) {
    return EXIT_FAILURE;
  }

  const bool useBenchmark = arguments::searchArgument(argc, argv, "--benchmark", true, nullptr);

  std::string useVsync;
  if (arguments::searchArgument(argc, argv, "--vsync", false, &useVsync) == -1) {
    std::cout << "--vsync requires a value" << std::endl;
    return EXIT_FAILURE;
  }

  //Create the window
  auto window = ammonite::windowManager::setupWindow(1024, 768, 4, "OpenGL Experiments");
  if (window == NULL) {
    return EXIT_FAILURE;
  }

  //Set an icon
  ammonite::windowManager::useIconDir(window, "assets/icons/");

  //Set vsync (disable if benchmarking)
  if (useVsync == "false" or useBenchmark) {
    ammonite::settings::graphics::setVsync(false);
  } else if (useVsync == "true") {
    ammonite::settings::graphics::setVsync(true);
  }

#ifdef DEBUG
  ammonite::utils::debug::enableDebug();
#endif

  //Enable engine caching, setup renderer and initialise controls
  bool success = true;
  ammonite::utils::cache::useDataCache("cache");
  ammonite::renderer::setup::setupRenderer(window, "shaders/", &success);
  ammonite::utils::controls::setupControls(window);

  //Graphics settings
  ammonite::settings::graphics::setGammaCorrection(true);

  //Renderer failed to initialise, clean up and exit
  if (!success) {
    std::cerr << "Failed to initialise renderer, exiting" << std::endl;
    cleanUp(0, nullptr);
    return EXIT_FAILURE;
  }

  //Load cube models
  ammonite::utils::Timer performanceTimer;
  const char* models[2] = {"assets/cube.obj", "assets/flat.png"};
  int modelCount = 10000;
  int loadedModelIds[modelCount];

  //Load model and apply a texture
  loadedModelIds[0] = ammonite::models::createModel(models[0], &success);
  ammonite::models::applyTexture(loadedModelIds[0], models[1], true, &success);
  long int vertexCount = ammonite::models::getVertexCount(loadedModelIds[0]);

  for (int i = 1; i < modelCount; i++) {
    //Load model and count vertices
    loadedModelIds[i] = ammonite::models::copyModel(loadedModelIds[0]);
    vertexCount += ammonite::models::getVertexCount(loadedModelIds[i]);
  }

  //Reposition cubes
  int sideLength = int(sqrt(modelCount));
  for (int x = 0; x < sideLength; x++) {
    for (int y = 0; y < sideLength; y++) {
      ammonite::models::position::setPosition(loadedModelIds[(x * sideLength) + y], glm::vec3(2.0f * float(x), 0.0f, 2.0f * float(y)));
    }
  }

  //Destroy all models, textures and shaders then exit
  if (!success) {
    cleanUp(modelCount, loadedModelIds);
    return EXIT_FAILURE;
  }

  std::cout << "Loaded models in: " << performanceTimer.getTime() << "s (" << vertexCount << " vertices)" << std::endl;

  //Create light sources
  int lightCount = 100;
  int lightSourceIds[lightCount];
  ammonite::lighting::setAmbientLight(glm::vec3(0.1f, 0.1f, 0.1f));

  glm::vec3 lightSourcePositions[lightCount];
  for (int i = 0; i < lightCount; i++) {
    lightSourceIds[i] = ammonite::lighting::createLightSource();
    lightSourcePositions[i] = glm::vec3((rand() % sideLength), 4.0f, (rand() % sideLength));
    float red = (rand() % 255 + 0) / 255.0f;
    float green = (rand() % 255 + 0) / 255.0f;
    float blue = (rand() % 255 + 0) / 255.0f;
    ammonite::lighting::properties::setPower(lightSourceIds[i], 50.0f);
    ammonite::lighting::properties::setColour(lightSourceIds[i], glm::vec3(red, green, blue));
  }

  //Camera ids
  int cameraIds[2] = {0, ammonite::camera::createCamera()};
  int cameraIndex = 0;
  bool cameraToggleHeld = false;

  //Set the camera to the start position
  ammonite::camera::setPosition(0, glm::vec3(0.0f, 0.0f, 5.0f));
  ammonite::camera::setPosition(cameraIds[1], glm::vec3(0.0f, 0.0f, 2.0f));

  //Performance metrics setup
  ammonite::utils::Timer benchmarkTimer;
  performanceTimer.reset();

  //Draw frames until window closed
  while(ammonite::utils::controls::shouldWindowClose()) {
    //Every second, output the framerate
    if (performanceTimer.getTime() >= 1.0f) {
      printMetrics(ammonite::renderer::getFrameTime());
      performanceTimer.reset();
    }

    //Handle toggling input focus
    static int lastInputToggleState = GLFW_RELEASE;
    int inputToggleState = glfwGetKey(window, GLFW_KEY_C);
    if (lastInputToggleState != inputToggleState) {
      if (lastInputToggleState == GLFW_RELEASE) {
        ammonite::utils::controls::setInputFocus(!ammonite::utils::controls::getInputFocus());
      }

      lastInputToggleState = inputToggleState;
    }

    //Cycle camera when pressed
    if (glfwGetKey(window, GLFW_KEY_B) != GLFW_PRESS) {
      cameraToggleHeld = false;
    } else if (!cameraToggleHeld) {
      cameraToggleHeld = true;
      cameraIndex = (cameraIndex + 1) % (sizeof(cameraIds) / sizeof(cameraIds[0]));
      ammonite::camera::setActiveCamera(cameraIds[cameraIndex]);
    }

    //Process new input since last frame
    ammonite::utils::controls::processInput();

    for (int i = 0; i < lightCount; i++) {
      bool invalid = true;
      int currLightSourceId = lightSourceIds[i];
      float x, z;
      while (invalid) {
        x = (rand() % 200 - 100) / 100.0f;
        z = (rand() % 200 - 100) / 100.0f;

        x += lightSourcePositions[i].x;
        z += lightSourcePositions[i].z;
        if (x >= 0.0f and x <= sideLength) {
          if (z >= 0.0f and z <= sideLength) {
            invalid = false;
          }
        }
      }

      lightSourcePositions[i] = glm::vec3(x, 4.0f, z);
      ammonite::lighting::properties::setGeometry(currLightSourceId, lightSourcePositions[i]);
    }

    ammonite::lighting::updateLightSources();

    //Draw the frame
    ammonite::renderer::drawFrame(loadedModelIds, modelCount);
  }

  //Output benchmark score
  if (useBenchmark) {
    std::cout << "\nBenchmark complete:" << std::endl;
    std::cout << "  Average fps: ";
    printMetrics(benchmarkTimer.getTime() / ammonite::renderer::getTotalFrames());
  }

  //Clean up and exit
  cleanUp(modelCount, loadedModelIds);
  return EXIT_SUCCESS;
}
