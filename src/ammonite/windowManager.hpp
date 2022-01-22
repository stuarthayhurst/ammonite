#ifndef WINDOW
#define WINDOW

namespace ammonite {
  namespace windowManager {
    namespace settings {
      void useVsync(bool enabled);
      bool isVsyncEnabled();
    }

    namespace setup {
      int setupGlfw(int antialiasing, float openglVersion);
      int setupGlew(GLFWwindow* window);
      void setupGlfwInput(GLFWwindow* window);
    }

    std::tuple<GLFWwindow*, int*, int*, float*> createWindow(int width, int height);
    void setTitle(GLFWwindow* window, const char title[]);

    //Wrapper for setup methods and createWindow()
    std::tuple<GLFWwindow*, int*, int*, float*> setupWindow(int newWidth, int newHeight, int antialiasing, float openglVersion, const char title[]);
  }
}

#endif
