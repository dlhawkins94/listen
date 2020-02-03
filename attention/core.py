import numpy as np
import scipy as sp
import tensorflow as tf

FRAME_DUR = 10 # ms

def load_frames(file_path):
    raw_audio = tf.io.read_file(file_path)
    (audio, sample_rate) = tf.audio.decode_wav(raw_audio, 1)

    (audio_N, _) = audio.shape
    frame_N = sample_rate * FRAME_DUR / 1000
    diff = frame_N - audio_N % frame_N
    if (diff > 0):
        audio = np.pad(audio, ((0, diff), (0, 0)), 'constant')
    
    audio = np.reshape(audio, (-1, frame_N))
    return np.transpose(audio)

def melsc(x):
    return 2595 * np.log10(1 + x/700.0)

def melinv(x):
    return 700 * (pow(10, x/2595.0) - 1)

class mel:
    def __init__(self, sample_rate, K):
        self.K = K
        self.fstart = 64
        self.fs = sample_rate
        
        self.cbin = [int(round(K * self.fstart / self.fs))]
        for i in range(1, 24):
            fc = melinv(melsc(self.fstart) + i *
                        (melsc(self.fs/2) - melsc(self.fstart))/24)
            self.cbin.append(int(round(K * fc / self.fs)))
        self.cbin.append(int(round(K / 2)))

    def apply(self, x):
        X = sp.fft(x, self.K)
        
        out = np.zeros((24))
        out[0] = np.log(np.sum(np.square(x)))        
        for i in range(1, 24):
            out_i = 0
            for k in range(self.cbin[i-1], self.cbin[i] + 1):
                out_i += (abs(X[k]) * (k - self.cbin[i-1] + 1)
                          / (self.cbin[i] - self.cbin[i-1] + 1))

            for k in range(self.cbin[i] + 1, self.cbin[i+1] + 1):
                out_i += (abs(X[k]) * (1 - (k - self.cbin[i])
                                       / (self.cbin[i+1] - self.cbin[i] + 1)))
                
            out[i] = np.log(out_i)

        return out;
