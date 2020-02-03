#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

#include <audiotk/audiotk.hpp>

#include "dataset.hpp"

using namespace std;

class stool {
  bool value;
  mutex mtx;
  
public:
  stool() : value(false) {}
  
  bool get() {
    mtx.lock();
    bool ret = value;
    mtx.unlock();
    return ret;
  }
 
  void set(bool val) {
    mtx.lock();
    value = val;
    mtx.unlock();
  }
};

// listener thread callback
void lst(stool *stopval) {
  pa_source in;
  denoise dn;
  vad v("model/vad");
  
  while (!stopval->get()) {
    frame x = in.read_frame();
    x = dn.apply(x);
    x = v.apply(x);
    static int col = 0;
    cout << x.flags << flush;
    if (++col == 80) {
      col = 0;
      cout << endl;
    }
  }
  stopval->set(false);
}

void listener() {
  server srv;
  stool stopval;
  thread lst_thr(lst, &stopval);

  string cmd;
  while (cmd != "quit") {
    cmd = srv.await_cmd();
      
    if (cmd == "suspend") {
      cout << "Suspending ASR." << endl;
      stopval.set(true);
      lst_thr.join();
    }
    else if (cmd == "resume") {
      cout << "Resuming ASR." << endl;
      lst_thr = thread(lst, &stopval);
    }
  }

  stopval.set(true);
  lst_thr.join();
}

void capture_sample(string path, int N) {
  pa_source in;
  denoise dn;
  file_sink out(path);

  // Prime the denoiser
  cout << "Priming..." << endl;
  for (int n = 0; n < NOISE_SAMPLE_LVL * 10; n++)
    dn.apply(in.read_frame());

  cout << "Recording sample." << endl;
  for (int n = 0; n < N; n++) {
    frame x = in.read_frame();
    x = dn.apply(x);
    out.write_frame(x);
  }
}

// The model size is defined by the number of vad sources you have.
void train_vad(int M, int N, int B) {
  cout << "Loading dataset..." << endl;

  vector<source*> srcs = load_file_sources("vad");
  vadset vset(srcs.size(), srcs);
  
  vset.load(N);

  cout << "Training vad gmms..." << endl;
  vad v(M);
  v.init(vset.get_frames());

  for (int b = 0; b < B; b++) {
    cout << "Epoch " << b << endl;
    vset.shuffle();
    v.train_gmm(vset.get_frames());
  }

  cout << "Training vad markov models..." << endl;
  v.train_mkv(load_track("data/vad_0_noise.wav", 1000), 0);
  v.train_mkv(load_track("data/vad_1_speech.wav", 1000), 1);
  v.train_mkv(load_track("data/vad_2_typing.wav", 1000), 2);
  //v.train_mkv(load_track("data/vad_3_speech.wav", 1000), 1);

  v.save("model/vad");
}

void scope() {
  pa_source in;
  denoise dn;
  cepstrum cep(256);
  vad v("model/vad");
  waterfall scope(v.size(), 100, 480, 480, 0, 1);
  pa_sink out;

  while (true) {
    frame x_(v.size());
    for (int n = 0; n < 5; n++) {
      frame x = in.read_frame();
      x = dn.apply(x);
      x = v.apply(x);
      x_[x.flags] += 1 / (double) 5;
    }
    int ind;
    x_.samples.real().maxCoeff(&ind);
    x_.samples *= 0.0;
    x_[ind] += 1.0;
    scope.write_frame(x_);
  }
}

int main(int argc, char **argv) {
  glfwInit();

  if (argc > 1) {
    string cmd(argv[1]);

    if (cmd == "capture_sample") {
      if (argc < 3) {
	cerr << "capture_sample: path N_frames" << endl;
	return 1;
      }

      capture_sample(string(argv[2]), atoi(argv[3]));
    }
    
    else if (cmd == "train_vad") {
      if (argc < 4) {
	cerr << "train_vad: N_models set_size N_epochs" << endl;
	return 1;
      }

      // I haven't picked a good way to see if the listener process is
      // actually running, yet. For now, just try to send a command. If
      // the socket isn't there then neither is the listener
      bool lstnr_present = send_cmd("suspend");
      train_vad(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
      
      if (lstnr_present) send_cmd("resume");
    }
    
    else if (cmd == "scope") scope();
  }
  
  else listener();

  glfwTerminate();
  return 0;
};
