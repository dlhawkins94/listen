#include "ui.hpp"

feedback_window::feedback_window()
  : win("speech_recognition", 24, 24)
{
  GLFWmonitor *primary = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode = glfwGetVideoMode(primary);
  win.w = mode->width - 2;
  glfwSetWindowSize(win, win.w, win.h);
  glfwSetWindowPos(win, 1, 1);

  vu = draw_volume(0.0);
  msg = print_text("Test");
}
