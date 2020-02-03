#include "statistics.hpp"

gmodel::gmodel(int K, int M)
  : K(K), pi(1.0/(double) M),
    mu(1 * VectorXd::Ones(K)),
    sig(MatrixXd::Identity(K,K))
{}

double gmodel::pdf(VectorXd x) {
  VectorXd x_ = x - mu;
  return exp(-0.5 * x_.transpose() * sig.inverse() * x_)
    / (pow(2*M_PI, (double) K/2) * sqrt(sig.determinant()));
}

gmm::gmm(int K, int M)
  : systm("gmm", {{1, M}}),
    B(0), K(K), M(M),
    models(M, gmodel(K, M))
{}

gmm::gmm(ifstream &file) : systm("gmm", {}), B(0) {
  file.read((char*) &K, sizeof(K));
  file.read((char*) &M, sizeof(M));
  this->add_dims({1, M});

  for (int m = 0; m < M; m++) {
    gmodel g(K, M);
    file.read((char*) &(g.pi), sizeof(double));
    
    for (int k = 0; k < K; k++) {
      double buf;
      file.read((char*) &buf, sizeof(double));
      g.mu(k) = buf;
    }

    for (int k1 = 0; k1 < K; k1++) {
      for (int k2 = 0; k2 < K; k2++) {
	double buf;
	file.read((char*) &buf, sizeof(double));
	g.sig(k1,k2) = buf;
      }	
    }

    models.push_back(g);
  }
}

// γ[m] = (π[m]N[m](x)) / (Σ π[m]N[m](x))
VectorXd gmm::guess(VectorXd x) {
  VectorXd gamma = VectorXd::Zero(M);
  double gamma_div = 0;

  for (int m = 0; m < M; m++) {
    gamma(m) = models[m].pi * models[m].pdf(x);
    gamma_div += gamma(m);
  }
  gamma /= gamma_div;

  return gamma;
}

void gmm::guess_helper(const MatrixXd &x, MatrixXd &gamma,
		       mutex &mtx, atomic<int> &cnt) {
  int n = cnt++;
  while (n < x.cols()) {
    VectorXd gamma_n = guess(x.col(n));
    mtx.lock();
    gamma.col(n) = gamma_n;
    mtx.unlock();
    n = cnt++;
  }
}

MatrixXd gmm::guess_multi(MatrixXd x) {
  int N = x.cols();
  MatrixXd gamma = MatrixXd::Zero(M,N);

  mutex mtx;
  atomic<int> cnt(0);
  vector<thread> thrs;
  for (int i = 0; i < N_THREADS; i++)
    thrs.push_back(thread(&gmm::guess_helper, this,
			  ref(x), ref(gamma), ref(mtx), ref(cnt)));

  for (auto &thr : thrs) thr.join();

  return gamma;
}

bus gmm::apply(bus b) {
  frame f_(M);
  VectorXd gamma = guess(b[0].samples.real());
    
  f_.samples = gamma.cast<complex<double>>();
  f_.flags = b[0].flags;

  return bus({f_});
}

// returns the row index of m which is closest to x.
MatrixXd::Index nearest(MatrixXd m, VectorXd x) {
  MatrixXd::Index ind;
  (m.rowwise() - x.transpose()).rowwise().squaredNorm().minCoeff(&ind);
  return ind;
}

void gmm::init_helper(const MatrixXd &x, const MatrixXd &mu_old, 
		      MatrixXd &mu, VectorXd &mag,
		      mutex &mtx, atomic<int> &cnt) {
  int n = cnt++;
  while (n < x.cols()) {
    int m = nearest(mu_old, x.col(n));
    
    mtx.lock();
    mag(m) += 1.0;
    mu.row(m) += x.col(n).transpose();
    mtx.unlock();
    
    n = cnt++;
  }
}

// naive k-means.
void gmm::init(dataset ds) {
  cout << "Initializing gmm means via kmeans." << endl;
  int N = ds.size();
  
  MatrixXd mu(M, K);
  VectorXd mag = VectorXd::Zero(M);

  // set up the initial means
  srand(time(NULL));
  for (int m = 0; m < M; m++) {
    int n = rand() % N;
    mu.row(m) = ds[n][0].samples.real().transpose();
  }

  MatrixXd x = MatrixXd::Zero(K, N);
  for (int n = 0; n < N; n++)
    x.col(n) = ds[n][0].samples.real();

  int steps = 0;
  while (true) {
    MatrixXd mu_old = mu;
    VectorXd mag_old = mag;
    
    mag *= 0.0;
    mu *= 0.0;

    // For each frame in the dataset, find the mean to which it is closest.
    // Group that frame with all the others which are closest to that mean.
    // Then take the mean in each group; the result is the new set of means.
    mutex mtx;
    atomic<int> cnt(0);
    vector<thread> thrs;
    for (int i = 0; i < N_THREADS; i++)
      thrs.push_back(thread(&gmm::init_helper, this,
			    ref(x), ref(mu_old), ref(mu), ref(mag),
			    ref(mtx), ref(cnt)));

    for (auto &thr : thrs) thr.join();
    
    for (int m = 0; m < M; m++)
      mu.row(m) /= mag(m);
    
    // When the number of frames in each group (mag) no longer changes, the
    // algorithm is assumed to have converged.
    steps++;
    if (mag_old == mag) break;
  }

  for (int m = 0; m < M; m++)
    models[m].mu = mu.row(m).transpose();

  cout << "k-means took " << steps << " steps." << endl;
}

// maximum likelihood training.
void gmm::train(dataset ds) {
  cout << "Epoch " << B++ << ": " << flush;
  int N = ds.size();
  int ndec = N / 10;
  
  MatrixXd x(K, N);
  for (int n = 0; n < N; n++)
    x.col(n) = ds[n][0].samples.real();

  // Get likelihoods for each model
  MatrixXd gamma = guess_multi(x);
  VectorXd gamma_sum = gamma.rowwise().sum();

  // Display the current log likelihood
  double likelihood = 0.0;
  for (int n = 0; n < N; n++)
    likelihood += log(gamma.col(n).maxCoeff());
  cout << "lh: " << likelihood << endl;

  // find new parameters, maximizing likelihood.
  for (int m = 0; m < M; m++) {
    models[m].pi = gamma_sum(m) / (double) N;

    models[m].mu *= 0;
    for (int n = 0; n < N; n++)
      models[m].mu += gamma(m,n) * x.col(n);
    models[m].mu /= gamma_sum(m);
    
    models[m].sig = MatrixXd::Zero(K,K);
    for (int n = 0; n < N; n++) {
      VectorXd diff = x.col(n) - models[m].mu;
      models[m].sig += gamma(m,n) * diff * diff.transpose();
    }
    models[m].sig /= gamma_sum(m);
  }
}

void gmm::save(ofstream &file) {
  file.write((char*) &K, sizeof(K));
  file.write((char*) &M, sizeof(M));
  
  for (int m = 0; m < M; m++) {
    file.write((char*) &(models[m].pi), sizeof(double));
    
    for (int k = 0; k < K; k++)
      file.write((char*) &(models[m].mu(k)), sizeof(double));

    for (int k1 = 0; k1 < K; k1++) {
      for (int k2 = 0; k2 < K; k2++)
	file.write((char*) &(models[m].sig(k1,k2)), sizeof(double));
    }
  }
}

deseq::deseq(deseq_params params)
  : systm("deseq", {}),
    params(params),
    pi(params.H, MatrixXd::Ones(params.M, params.M)),
    state(VectorXi::Zero(params.T)),
    state_i(0)
{}

deseq::deseq(ifstream &file)
  : systm("deseq", {}),
    state_i(0)
{
  file.read((char*) &params, sizeof(params));
  for (int h = 0; h < params.H; h++) {
    MatrixXd pi_h = MatrixXd::Zero(params.M, params.M);
    for (int m0 = 0; m0 < params.M; m0++) {
      for (int m1 = 0; m1 < params.M; m1++) {
	double buf;
	file.read((char*) &buf, sizeof(buf));
	pi_h(m0,m1) = buf;
      }
    }
    pi.push_back(pi_h);
  }
  state = VectorXi::Zero(params.T);
}

bus deseq::apply(bus b) {
  // get current state of m
  int m, nul;
  b[0].samples.real().maxCoeff(&m, &nul);
  state(state_i++) = m;
  if (state_i == params.T) state_i = 0;

  // state_i now marks the earliest element in the buffer.
  VectorXd score = VectorXd::Ones(params.H);
  int j = state_i;
  int t = 0;
  int m0, m1;
  
  m1 = state(j++);
  for (t = 1; t < params.T; t++) {
    if (j == params.T) j = 0;
    m0 = m1;
    m1 = state(j++);

    // score(h) : (# times m0 -> m1 while h_x) / (# times m0 -> m1 ∀ h)
    // basically, how often did m0 -> m1 while h was the signal type,
    // compared to all the other times m0 -> m1
    //
    // To extend this principle to the whole sequence, simply multiply
    // all those scores against each other
    // The result doesn't seem to be a probability (I think I got math wrong
    // somewhere) but it does bear meaningful results, which show some
    // likelihood that the sequence matches one model rather than the other
    // For now I just normalize the output of deseq.
    double total = 0.0;
    for (int h = 0; h < params.H; h++) {
      score(h) *= pi[h](m0, m1);
      total += pi[h](m0, m1);
    }
    score /= total;
  }

  bus b_({frame(params.H)});
  b_[0].samples = score.cast<complex<double>>();
  return b_;
}

// fitting each markov model is very simple. For each instance of m0 -> m1 for
// model h, increment that coefficient in h's transition matrix.
void deseq::train(vector<frame> track, int h) {
  int m0, m1, nul;
  track[0].samples.real().maxCoeff(&m1, &nul);
  for (int n = 1; n < track.size(); n++) {
    m0 = m1;
    track[n].samples.real().maxCoeff(&m1, &nul);
    pi[h](m0,m1) += 1;
  }
}

void deseq::save(ofstream &file) {
  file.write((char*) &params, sizeof(params));
  for (int h = 0; h < params.H; h++) {
    for (int m0 = 0; m0 < params.M; m0++) {
      for (int m1 = 0; m1 < params.M; m1++) {
	double buf = pi[h](m0,m1);
	file.write((char*) &buf, sizeof(buf));
      }
    }
  }
}
