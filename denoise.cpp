#include "denoise.hpp"

VectorXd hanning(int N) {
  VectorXd w = VectorXd::Zero(N);
  for (int n = 0; n < N; n++) {
    double n_ = n + 0.5;
    w(n) = 0.5 * (1 - cos(2 * M_PI * n_/N));
  }
  return w;
}

VectorXd blackman(double alpha, int N) {
  double a0 = (1 - alpha) / 2;
  double a1 = 0.5;
  double a2 = alpha / 2;

  VectorXd w = VectorXd::Zero(N);
  for (int n = 0; n < FRAME_N; n++) {
    double n_ = n + 0.5;
    w[n] = a0 - a1 * cos(2* M_PI * n_ / N)
      + a2 * cos(4 * M_PI * n_ / N);
  }
  return w;
}

denoise::denoise(int update_ivl, double alpha_D)
  : K(256),
    update_ivl(update_ivl), upd(0),
    alpha_D(alpha_D),
    w(hanning(FRAME_N)),
    D(VectorXd::Zero(K)),
    dft(K), idft(K)
{
}

denoise::denoise(ifstream &file)
  : K(256),
    alpha_D(alpha_D),
    w(hanning(FRAME_N)),
    upd(0),
    D(VectorXd::Zero(K)),
    dft(K), idft(K)
{
  file.read((char*) &update_ivl, sizeof(update_ivl));
  file.read((char*) &alpha_D, sizeof(alpha_D));
}

frame denoise::take_dft(frame f) {
  frame f_(256);
  for (int k = 0; k < f.samples.size(); k++)
    f_[k] = f[k] * w(k);
  
  return dft.apply({f_})[0];
}

frame denoise::take_idft(frame F) {
  frame f_ = idft.apply({F})[0];
  
  frame f(FRAME_N);
  for (int k = 0; k < FRAME_N; k++) {
    if (abs(w(k)) > 0.0)
      f[k] = f_[k];// / w(k);
  }

  return f;
}

frame denoise::apply_once(frame f) {
  complex<double> I(0.0, 1.0);
  frame F = take_dft(f);

  frame F_(K);
  for (int k = 0; k < K; k++) {
    double Mg = abs(F[k]);
    double Ph = arg(F[k]);
    if (Mg >= D(k))
      F_[k] = (Mg - D(k)) * exp(I * Ph);
    else F_[k] = 0.0;
  }

  return take_idft(F_);
}

void denoise::update(frame f) {
  if (upd == update_ivl) {
    upd = 0;
    frame F = take_dft(f);
    D = (1 - alpha_D) * D + alpha_D * F.samples.cwiseAbs();
  }
  upd++;
}

void denoise::save(ofstream &file) {
  file.write((char*) &update_ivl, sizeof(int));
  file.write((char*) &alpha_D, sizeof(double));
}

denoise2::denoise2(denoise2_params params)
  : params(params),
    wt(params.N, 512, "D8"),
    iwt(params.N, 512, "D8")
{}

int quickselect(VectorXd v, vector<int> ind, int K) {
  int N = ind.size();
  int k = ind[rand() % N];

  vector<int> los, his, eqs;
  for (auto &i : ind) {
    if (v(i) < v(k))
      los.push_back(i);
    else if (v(i) > v(k))
      his.push_back(i);
    else eqs.push_back(i);
  }
  
  if (K < los.size())
    return quickselect(v, los, K);
  else if (K < los.size() + eqs.size())
    return eqs.front();
  else
    return quickselect(v, his, K - los.size() - eqs.size());
}

// Only works with odd kernel size
double denoise2::median(VectorXd v) {
  int N = v.size();
  srand(time(NULL));
  vector<int> vec;
  for (int i = 0; i < N; i++)
    vec.push_back(i);

  stopwatch st;
  st.start();
  int k = quickselect(v, vec, N/2);
  st.stop();
  if (st.dur() > 10)
    cout << st.dur() << endl;
  return v(k);
}

frame denoise2::apply_once(frame f) {
  bus b = wt.apply({f});
  int R = 4;

  for (int n = 0; n < b.size() - 1; n++) {
    VectorXd mag = b[n].samples.cwiseAbs();

    int K = b[n].samples.size();
    if (n < params.Nmed) {
      for (int k = 0; k < K; k++) {
	int k0 = k - R;
	if (k0 < 0) k0 = 0;
	int k1 = k + R;
	if (k1 >= K) k1 = K - 1;

	VectorXd mag_k = mag.segment(k0, k1 - k0 + 1);
	double tau_k = params.beta[n] * median(mag_k);
	if (mag[k] > tau_k)
	  b[n][k] *= tau_k / mag[k] / params.beta[n];
      }
    }
    else {
      for (int k = 0; k < K; k++) {
	if (mag[k] < params.beta[n])
	  b[n][k] *= 0.0;
      }
    }
  }

  f = iwt.apply(b)[0];
  f.samples *= 1.0;
  return f;
}
