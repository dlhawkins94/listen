#pragma once

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

#include <audiotk/audiotk.hpp>

#include "dataset.hpp"
#include "statistics.hpp"

// K :: size of input frames.
// M :: number of gmm classes.
// H :: number of output states.
// T :: length of deseq buffer.
// atk_N: length of the attack buffer.
// hng_N: length of the hangover decay.
struct vad_params {
  int K, M, H, T;
  int atk_N, hng_N;
};

/*
 * Simple vad that combines a gaussian mixture model and several markov models
 * to classify a signal as noise, speech, or impulse. The gmm fits the cepstrum
 * of each frame to its gaussian dists, yielding a sequence of model indices
 * (m). This sequence is then fit by deseq against one of H markov models; the
 * result is that h = noise, speech, or impulse.
 *
 * The vad uses an 'attack' gate which requires a certain number of consecutive
 * speech frames before the input can be considered speech; this prevents small
 * blips from being registered as the beginning of a speech segment. There is
 * also a hangover mechanism, which is basically a decay. This allows for some
 * gap in speech detection; as long as more speech is registered before the
 * hangover runs out, the vad won't cut off its output.
 *
 * This approach is decent, though it has random pitfalls. Some types of
 * sound didn't make into the impulse noise datasets, so these are sometimes
 * registered as voice. Although the vad should be robust to loudness, sometimes
 * changing the microphone gain upsets one of the models. Logic dictates that
 * the cepstrum is somehow different in character when the loudness changes,
 * which causes a different gmm sequence. This is hard to verify, but if it's
 * indeed the case, then it's not something I can really fix, other than
 * changing to a different domain (i.e., wavelets).
 *
 * Overall, the result is that it nearly perfectly classifies background noise,
 * it knows that typing is impulse noise (except maybe an odd-sounding lone
 * keystroke), random thumps and bangs are usually classed as impulse.
 * It usually detects speech; as long as you speak loud enough to overcome the
 * SNR, it catches on and usually catches all ensuing speech segments. Pure
 * noise never causes false alarms, but some impulse noise occasionally does.
 * It also thinks heavy breathing and sighs are speech... I don't want to create
 * an entire new output class and breath into a mic for 5 minutes to fix that.
 *
 * My next approach will be to try to use wavelets and maybe some neural nets.
 */
class vad : public systm {
  vad_params params;
  
  cepstrum cep; // cepstral coefficient transform.
  gmm g; // gaussian mixture model classifier.
  deseq d; // markov model classifier.

  VectorXi state; // attack state. When full, output is classified as speech.
  int state_i; // attack state buffer index.
  int hng_state; // hangover state. Decays, allowing for some gap in speech.

  queue<frame> framebuffer;

public:
  vad(vad_params params);
  vad(ifstream &file);
  vad_params par() { return params; }
  void set_par(int T, int atk_N, int hng_N) {
    d.set_par(T);
    params.atk_N = atk_N;
    params.hng_N = hng_N;
  }
  bus apply(bus b);

  void train_gmm(int N_epochs); // inits and trains the gmm.
  void train_mkv(); // trains the markov models.
  void save(ofstream &file);

  dataset load_gmm_dataset(); // loads a bunch of frames to train gmm.
};
