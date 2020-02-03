#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <audiotk/audiotk.hpp>

#include "dataset.hpp"

using namespace std;

#define N_THREADS 4

/*
 * One multivariate gaussian distribution.
 */
struct gmodel {
  int K;
  double pi;
  VectorXd mu;
  MatrixXd sig;

  gmodel(int K, int M);
  double pdf(VectorXd x);
};

/*
 * Gaussian mixture model. Input frames of width K are tested against
 * each gaussian dist to determine which dist would most likely produce each
 * frame. The output is a vector of likelihoods; typically this is argmax'd
 * to get a categorical output.
 */
class gmm : public systm {
  int K, M, B; // K :: input width, M :: number of models
  vector<gmodel> models;

  // for each dist, gets the probability that x fits it.
  VectorXd guess(VectorXd x);
  MatrixXd guess_multi(MatrixXd x); // threaded version.

  // multithreading helper functions.
  void guess_helper(const MatrixXd &x, MatrixXd &gamma,
		    mutex &mtx, atomic<int> &cnt);

  void init_helper(const MatrixXd &x, const MatrixXd &mu_old, 
		   MatrixXd &mu, VectorXd &mag,
		   mutex &mtx, atomic<int> &cnt);

public:
  gmm(int K, int M);
  gmm(ifstream &file);
  int get_M() { return M; }
  bus apply(bus b);

  void init(dataset ds); // initializes the means of the models w/ k-means
  void train(dataset ds); // fits the models to the dataset using ML
  void save(ofstream &file);
};

/*
 * M :: input width
 * H :: output width (# of output states)
 * T :: input sequence length
 */
struct deseq_params {
  int M, H, T;
};

/*
 * deseq (name has no meaning, picked at 2 AM) observes an input sequence
 * and determines which of H markov models would best fit that sequence.
 * it's not quite an HMM even though I called the output H; there is no
 * hidden state, just a bunch of markov models and their likelihoods. 
 */
class deseq : public systm {
  deseq_params params;
  vector<MatrixXd> pi; // the markov models. Each matrix is transition probs.
  VectorXi state; // state buffer, of length T
  int state_i; // index of state buffer

public:
  deseq(deseq_params params);
  deseq(ifstream &file);
  deseq_params par() { return params; }
  void set_par(int T) { params.T = T; }
  bus apply(bus b);

  void train(vector<frame> track, int h); // sets up transition probabilities

  void save(ofstream &file);
};
