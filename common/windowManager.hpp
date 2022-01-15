#ifndef WINDOW
#define WINDOW

namespace windowManager {
  namespace setup {
    int setupGlfw(int antialiasing, int openglMajor, int openglMinor);
    int setupGlew();
    void setupGlfwInput(GLFWwindow* window);
  }

  GLFWwindow* createWindow(int width, int height, const char title[]);
}

#endif