#include "vad.hpp"

vad::vad(vad_params params)
  : systm("vad", {{params.H, 1}}),
    params(params),
    cep(256),
    g(12, params.M),
    d({.M = params.M, .H = params.H, .T = params.T}),
    state(VectorXi::Zero(params.atk_N)),
    state_i(0),
    hng_state(0)
{}

vad::vad(ifstream &file)
  : systm("vad", {}),
    cep(256),
    g(file),
    d(file),
    state_i(0),
    hng_state(0)
{
  file.read((char*) &params, sizeof(params));
  add_dims({params.H, 1});
  state = VectorXi::Zero(params.atk_N);
}

bus vad::apply(bus b) {
  bus gn = g.apply(cep.apply({b[0]}));
  bus dn = d.apply(gn);
  dn[0].samples.normalize();

  int h, nul;
  dn[0].samples.real().maxCoeff(&h, &nul);
  state(state_i++) = (int) (h == 1);
  if (state_i == params.atk_N)
    state_i = 0;

  if (state.sum() >= params.atk_N)
    hng_state = params.hng_N;
  else if (hng_state) hng_state--;

  int h_out;
  if (hng_state) h_out = 1;
  else if (h != 1) h_out = h;
  else h_out = 0;

  framebuffer.push(b[0]);
  if (h_out != 1) {
    while (framebuffer.size() > 2 * params.T)
      framebuffer.pop();
  }

  b = {frame(1), dn[0], gn[0]};
  b[0][0] = complex<double>(h_out, 0.0);
  return b;
}

void vad::train_gmm(int N_epochs) {
  dataset ds = load_gmm_dataset();
  g.init(ds);
  for (int b = 0; b < N_epochs; b++) {
    shuffle_dataset(ds);
    g.train(ds);
  }
}

void vad::train_mkv() {
  d = deseq({.M = params.M, .H = params.H, .T = params.T});
  
  vector<file_source*> srcs = load_file_sources("vad");
  for (auto &src : srcs) {
    file_source_info info(src);
    cout << "Training mkv with src " << info.desc << endl;
    
    vector<frame> track;
    bus b = src->apply({});
    while (!b.empty()) {
      track.push_back(g.apply(cep.apply(b))[0]);
      b = src->apply({});
    }
    
    int h = info.sigtype;
    delete src;
    
    d.train(track, h);
  }
}

void vad::save(ofstream &file) {
  g.save(file);
  d.save(file);
  file.write((char*) &params, sizeof(params));
}

dataset vad::load_gmm_dataset() {
  cout << "loading vad gmm dataset." << endl;
  dataset ds;
  
  vector<file_source*> srcs = load_file_sources("vad");
  for (auto &src : srcs) {
    vector<bus> track;

    // get all frames from the given source.
    bus b = src->apply({});
    while (!b.empty()) {
      b.push_back(frame(1));
      b.push_back(frame(2));
      b[1][0] = complex<double>(frame_power(b[0]), 0.0);
      b[2][0] = complex<double>(file_source_info(src).sigtype, 0.0);
      track.push_back(b);
      
      b = src->apply({});
    }
    delete src;

    for (auto &b_ : track) {
      b_[0] = cep.apply_siso(b_[0]);
      ds.push_back(b_);
    }
  }

  shuffle_dataset(ds);
  return ds;
}

