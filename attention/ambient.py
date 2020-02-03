import numpy as np
import os
import pathlib
import pickle
import scipy as sp
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'
import tensorflow as tf

from core import *

data_dir_path = "../data/TUT_acoustic_scenes_2017/"

def load_meta(dpath, which_set):
    filepath = dpath + which_set + "_meta/meta.txt"
    tracks = []
    lookup = {}
    with open(filepath) as file:
        for line in file:
            [path, catg, _] = line.split()
            name = path.split("/")[-1]
            
            if not catg in lookup:
                lookup[catg] = len(lookup)
            tracks.append((name, catg))
                
    return tracks, lookup

class TUTGenerator(tf.keras.utils.Sequence):
    def __init__(self, dpath, which_set,
                 batch_size = 8,
                 sample_size = 1001):
        
        self.indir = dpath + which_set + "_mel/"
        self.tracks, self.lookup = load_meta(dpath, which_set)
        self.N = len(self.tracks)
        self.batch_size = batch_size
        self.sample_size = sample_size
        self.feature_len = 24

        self.data = []
        self.indices = []
        cnt = -1
        for name, catg in self.tracks:
            spectra = np.load(self.indir + name + ".mel.npy")
            (_, N) = spectra.shape
            p = self.lookup[catg]
            self.data.append((spectra, p))
            cnt += 1
            self.indices.append(cnt)
            #k = 0
            #while (k < N - self.sample_size):
            #    self.data.append((spectra[:, k:(k + self.sample_size)], p))
            #    k += self.sample_size

        np.random.shuffle(self.indices)

    def depth(self):
        return len(self.lookup)
        
    def __len__(self):
        return int(np.floor(len(self.data) / self.batch_size))
    
    def __getitem__(self, ind):
        x = np.zeros((self.batch_size, self.sample_size, self.feature_len))
        y = np.zeros((self.batch_size), dtype=int)
        
        for b in range(0, self.batch_size):
            xb, yb = self.data[self.indices[ind + b]]
            tf.keras.utils.normalize(xb, 0)
            x[b,] = xb.transpose()
            y[b,] = yb

        np.random.shuffle(self.indices)
        return (x, y)

    def on_epoch_end(self):
        np.random.shuffle(self.indices)
            
training_generator = TUTGenerator(data_dir_path, "training")
x, y = training_generator[0]
(B, K, M) = x.shape

# K is the sequence length -- gradient is calculated that far
# back in time. For random background noises we only need really recent
# information.
model = tf.keras.models.Sequential()
model.add(tf.keras.layers.LSTM(64, input_shape=(K, M)))
model.add(tf.keras.layers.Dense(training_generator.depth(),
                                input_shape=(1,64),
                                activation='softmax'))

model.compile(optimizer = 'adam',
              loss = 'sparse_categorical_crossentropy',
              metrics = ['accuracy'])

model.summary()
model.fit_generator(generator = training_generator,
                    steps_per_epoch = 15,
                    epochs = 20,
                    use_multiprocessing = True,
                    workers = 4)

test_generator = TUTGenerator(data_dir_path, "test")
model.evaluate_generator(generator = test_generator,
                         max_queue_size = len(test_generator) / 4,
                         verbose = 1,
                         use_multiprocessing = True,
                         workers = 4)

