#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include <audiotk/audiotk.hpp>

#include "dataset.hpp"
#include "statistics.hpp"

using namespace std;

/*
 * Removes noise by subtracting a noise estimate from the magnitude of the
 * signal's fft. The denoiser takes an initial estimate by averaging the
 * amplitude of several frames of noise. It then modifies the estimate
 * over time, so the denoiser keeps track of general changes in the noise
 * we're trying to remove.
 *
 * Frames which are too loud are considered to not be noise, so they aren't
 * used in the estimate. By nature this excludes impulse noise so that needs to
 * be removed with VAD. If the SNR is too low the system won't work very well 
 * since hard spectra removal will cause distortion and remove a lot of speech
 * signal.
 *
 * This system uses overlapping frames. See overlap base class in dsp_core.
 * 
 * Each frame is windowed before its fft is taken, and is dewindowed after its
 * ifft is taken post-proc.
 */
class denoise : public overlap {
  int K; // FFT width
  int update_ivl;
  double alpha_D;
  VectorXd w; // window function
  fft dft; ifft idft;

  int upd;
  VectorXd D; // Magnitude of noise estimate

  frame take_dft(frame f);
  frame take_idft(frame F);
  
public:
  denoise(int update_ivl, double alpha_D);
  denoise(ifstream &file);
  int get_update_ivl() { return update_ivl; }
  double get_alpha_D() { return alpha_D; }
  frame apply_once(frame f); // Denoise one frame.
  
  void update(frame f);
  void save(ofstream &file);
};

struct denoise2_params {
  int N, Nmed;
  vector<double> beta;
};

class denoise2 : public overlap {
  denoise2_params params;
  fwt wt; ifwt iwt;
  double median(VectorXd v);

public:
  denoise2(denoise2_params params);
  frame apply_once(frame f);
};
