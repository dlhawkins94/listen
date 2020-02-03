#include <fstream>
#include <iostream>
#include <map>
#include <vector>

#include <sys/time.h>

#include <Eigen/Core>

#include <audiotk/audiotk.hpp>

#include "dataset.hpp"
#include "denoise.hpp"
#include "statistics.hpp"
#include "vad.hpp"

using namespace std;

void run() {
  pa_source in;
  denoise2 dn({.N = 8,
	       .Nmed = 5,
	       .beta = {0.05, 0.05, 0.05, 0.05, 0.05, 0.15, 0.10, 0.10}});

  /*
  fwt wt(8, 512, "D8");
  multiscope scope2(1024, 768,
		    {256, 128, 64, 32, 16, 8, 4, 2, 2},
		    vector<double>(9, 0.0),
		    vector<double>(9, 0.5));
  */
  pa_sink out;

  int oof = 0;
  while (true) {
    bus b = in.apply({});
    b = dn.apply(b);

    /*
    bus b_ = wt.apply(b);
    for (auto &f : b_)
      f.samples = f.samples.cwiseAbs().cast<complex<double>>();
    scope2.apply(b_);
    */
    out.apply(b);
  }
}

int main(int argc, char **argv) {
  Eigen::setNbThreads(4);
  Eigen::initParallel();
  glfwInit();

  if (argc > 1) {
    cout << "invalid argument" << endl;
  }
  else run();

  glfwTerminate();
  
  return 0;
}
