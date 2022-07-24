#include <iostream>
#include <cstdlib>
#include <string>

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

  //Load models from a set of objects and textures
  const char* models[][2] = {
    {"assets/suzanne.obj", "assets/gradient.png"},
    {"assets/cube.obj", "assets/flat.png"}
  };
  int modelCount = sizeof(models) / sizeof(models[0]);
  int maxNewModels = 25;
  int loadedModelIds[modelCount + maxNewModels + 1];

  long int vertexCount = 0;
  ammonite::utils::Timer performanceTimer;
  for (int i = 0; i < modelCount; i++) {
    //Load model
    loadedModelIds[i] = ammonite::models::createModel(models[i][0], &success);

    //Count vertices
    vertexCount += ammonite::models::getVertexCount(loadedModelIds[i]);

    //Load texture
    ammonite::models::applyTexture(loadedModelIds[i], models[i][1], true, &success);
  }

  //Copy last loaded model
  loadedModelIds[modelCount] = ammonite::models::copyModel(loadedModelIds[modelCount - 1]);
  vertexCount += ammonite::models::getVertexCount(loadedModelIds[modelCount]);
  ammonite::models::position::setPosition(loadedModelIds[modelCount], glm::vec3(4.0f, 4.0f, 4.0f));
  ammonite::models::position::scaleModel(loadedModelIds[modelCount], 0.25f);
  modelCount += 1;

  //Example translation, scale and rotation
  ammonite::models::position::translateModel(loadedModelIds[0], glm::vec3(-2.0f, 0.0f, 0.0f));
  ammonite::models::position::scaleModel(loadedModelIds[0], 0.8f);
  ammonite::models::position::rotateModel(loadedModelIds[0], glm::vec3(0.0f, 0.0f, 0.0f));

  //Destroy all models, textures and shaders then exit
  if (!success) {
    cleanUp(modelCount, loadedModelIds);
    return EXIT_FAILURE;
  }

  std::cout << "Loaded models in: " << performanceTimer.getTime() << "s (" << vertexCount << " vertices)" << std::endl;

  //Set light source properties
  int lightId = ammonite::lighting::createLightSource();
  ammonite::lighting::properties::setPower(lightId, 50.0f);
  ammonite::lighting::linkModel(lightId, loadedModelIds[modelCount - 1]);
  ammonite::lighting::updateLightSources();
  ammonite::lighting::setAmbientLight(glm::vec3(0.1f, 0.1f, 0.1f));

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

    //Spawn object
    static bool spawnToggleHeld = false;
    static int newModels = 0;
    if (glfwGetKey(window, GLFW_KEY_F) != GLFW_PRESS) {
      spawnToggleHeld = false;
    } else if (!spawnToggleHeld) {
      spawnToggleHeld = true;
      if (newModels < maxNewModels) {
        int activeCameraId = ammonite::camera::getActiveCamera();
        loadedModelIds[modelCount] = ammonite::models::copyModel(loadedModelIds[1]);

        float horiz = glm::degrees(ammonite::camera::getHorizontal(activeCameraId));
        float vert = glm::degrees(ammonite::camera::getVertical(activeCameraId));

        ammonite::models::position::setRotation(loadedModelIds[modelCount], glm::vec3(vert, horiz, 0.0f));
        ammonite::models::position::setScale(loadedModelIds[modelCount], 0.25f);
        ammonite::models::position::setPosition(loadedModelIds[modelCount], ammonite::camera::getPosition(activeCameraId));

        std::cout << "Spawned cube" << std::endl;
        modelCount++;
        newModels++;
      }
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
