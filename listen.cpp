#include "listen.hpp"

vector<double> hanning(int N) {
  vector<double> w(N, 0);
  for (int n = 0; n < N; n++) {
    double n_ = n + 0.5;
    w[n] = 0.5 * (1 - cos(2 * M_PI * n_/N));
  }
  return w;
}

vector<double> blackman(double alpha, int N) {
  double a0 = (1 - alpha) / 2;
  double a1 = 0.5;
  double a2 = alpha / 2;

  vector<double> w(N, 0);
  for (int n = 0; n < FRAME_N; n++) {
    double n_ = n + 0.5;
    w[n] = a0 - a1 * cos(2* M_PI * n_ / N)
      + a2 * cos(4 * M_PI * n_ / N);
  }
  return w;
}

denoise::denoise()
  : K(NOISE_K),
    sample_size(0),
    last_sample(0),
    A0(0),
    w(hanning(FRAME_N)),
    Mn(K, 0),
    dft(K), idft(K) {}

frame denoise::apply_once(frame f) {
  // Window and pad the input frame
  frame s(K);
  for (int n = 0; n < FRAME_N; n++)
    s[n] = f[n] * w[n];

  frame S = dft.apply(s);

  // If true, we've sampled enough noise for an estimate
  bool ready = sample_size == NOISE_SAMPLE_LVL;

  // If nosig true, amplitude of this frame is close enough to the last one
  // and it's quiet enough to be considered noise.
  double A = amplitude(f);
  bool nosig = A < NOISE_MAX_AMPL && abs(A - A0) < NOISE_DELTA_THRESH;
  A0 = A; // shift in current amplitude
  
  for (int k = 0; k < K; k++) {
    double Mk = abs(S[k]); // mag
    double Thk = arg(S[k]); // phase

    if (nosig) { // Frame is probably noise, so use it to estimate
      if (!ready) // add to estimate if it's not ready
	Mn[k] += Mk / NOISE_SAMPLE_LVL;

      // If sample level has been reached then adjust estimate
      else if (last_sample == NOISE_SAMPLE_LVL)
	Mn[k] = ((NOISE_SAMPLE_LVL - 1) * Mn[k] + Mk) / NOISE_SAMPLE_LVL;
    }

    if (ready) { // Subtract noise estimate if it's ready
      double dM = (Mk - Mn[k]); // * (Mi > Mn[i]); // <- zero-floor
      S[k] = dM * exp(complex<double>(0.0,1.0) * Thk);
    }
  }

  // Modify the sample size if initial estimate still being built.
  // If the estimate is complete, modify time since last resample.
  if (nosig) {
    if (!ready) sample_size++;
    else if (last_sample == NOISE_SAMPLE_LVL)
      last_sample = 0;
    else last_sample++;
  }

  // Return unmodified frames until estimate is ready.
  if (nosig && !ready) return f;
  else {
    frame s_ = idft.apply(S);
    frame f_(FRAME_N);
    for (int n = 0; n < FRAME_N; n++) {
      f_[n] = s_[n] * w[n];
    }
    return f_;
  }
}

iir::iir(vector<double> a, vector<double> b)
  : K(b.size()), a(a), b(b)
{
  for (int k = 0; k < K; k++)
    v.push_front(0);
}

frame iir::apply(frame f) {
  int N = f.samples.size();
  frame f_(f.samples);

  // direct form ii
  v.pop_back();
  for (auto &fn : f_.samples) {
    complex<double> vn = fn;
    for (int k = 1; k < K; k++)
      vn -= a[k] * v[k - 1];
    v.push_front(vn);

    fn = 0;
    for (int k = 0; k < K; k++)
      fn += b[k] * v[k];
  }
  return f_;
}

normal_dist::normal_dist(int K) : K(K), mu(K), sig(K,K) {}

double normal_dist::pdf(frame f) {
  VectorXd x(K);
  for (int k = 0; k < K; k++)
    x(k) = real(f[k]);

  VectorXd x_ = x - mu;
  return exp(-0.5 * x_.transpose() * sig.inverse() * x_)
    / (pow(2*M_PI, K/2) * sqrt(sig.determinant()));
}

vad::vad() : noise_model(24), speech_model(24), pnoise(0.5), pspeech(0.5) {}

void vad::train(int z, frame f) {
}

double vad::guess(frame f) {
  double p_x_n = pnoise * noise_model.pdf(f);
  double p_x_s = pspeech * speech_model.pdf(f);

  double p_s = p_x_s/(p_x_n + p_x_s);
  return p_s;
}
