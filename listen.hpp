#pragma once

#include <deque>
#include <iostream>
#include <vector>
#include <unistd.h>

#include <eigen3/Eigen/Dense>

#include "dsp_core.hpp"

// Size of the fft. Frame is padded to fit this.
#define NOISE_K 256

// number of frames that are sampled before noise reduction is applied
// After this point, it defines the scaling factor of any later frames.
// Additionally, the rate of adjustment to the noise estimate is reduced.
#define NOISE_SAMPLE_LVL 50

// if the amplitude of the first frame differs from the second by this
// much then one or the other possibly contains signal. Don't use either to
// estimate noise.
#define NOISE_DELTA_THRESH 0.2

// Max value a frame can have for it to be considered noise.
#define NOISE_MAX_AMPL 0.25

using namespace std;
using namespace Eigen;

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
  int sample_size; // Initial sample count.
  int last_sample; // Time since last resample (when estimate is ready).
  vector<double> w; // window function
  vector<double> Mn; // Magnitude of noise estimate
  double A0; // Amplitude of the last frame.
  fft dft; ifft idft;
  
public:
  denoise();
  frame apply_once(frame f); // Denoise one frame.
};

class iir : public system {
  int K;
  vector<double> a, b;
  deque<complex<double>> v;

public:
  iir(vector<double> a, vector<double> b);
  frame apply(frame f);
};

struct normal_dist {
  int K;
  VectorXd mu;
  MatrixXd sig;

  normal_dist(int K);
  double pdf(frame f);
};

class vad {
  normal_dist noise_model, speech_model;
  double pnoise, pspeech;

public:
  vad();
  
  void train(int z, frame f);
  double guess(frame f);
};
