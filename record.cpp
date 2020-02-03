#include <cstdlib>
#include <iostream>

#include <audiotk/audiotk.hpp>

#include "denoise.hpp"

int main(int argc, char **argv) {
  string path(argv[1]);
  int N = atoi(argv[2]);

  pa_source in;
  denoise dn;
  file_sink out(path);

  cout << "Sampling noise..." << endl;
  for (int n = 0; n < 50; n++)
    dn.apply(in.apply({}));

  cout << "Recording..." << endl;
  for (int n = 0; n < N; n++) {
    bus b = dn.apply(in.apply({}));
    out.apply(b);
  }
  
  return 0;
}
