#!/usr/bin/env python

# Copyright 2016-17 The RamFuzz contributors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""A sample Keras model trainable on the output of ./gencorp.py.  It tries to
predict the test success or failure based on the logged RamFuzz values during
the test run.  Using a simple CNN, it can achieve >99% accuracy.  The model is
adapted from Alexander Rakhlin's sample implementation of NLP CNN:
https://github.com/alexander-rakhlin/CNN-for-Sentence-Classification-in-Keras

Usage: $0 [epochs] [batch_size]
Defaults: epochs=1, batch_size=50
Expects a train/ subdirectory containing the output of ./gencorp.py.

"""

from keras.constraints import min_max_norm
from keras.layers import BatchNormalization, Conv1D, Dense, Dropout
from keras.layers import Embedding, Flatten, Input, MaxPooling1D
from keras.layers.merge import concatenate
from keras.metrics import binary_crossentropy
from keras.models import Model
from keras.optimizers import Adam
import glob
import keras.backend as K
import os.path
import rfutils
import sys

gl = glob.glob(os.path.join('train', '*.[sf]'))
poscount, locidx = rfutils.count_locpos(gl)

embedding_dim = 4
filter_sizes = (3, 8)
num_filters = 1
dropout_prob = (0.01, 0.01)
hidden_dims = 10

K.set_floatx('float64')

in_vals = Input((poscount, 1), name='vals', dtype='float64')
normd = BatchNormalization(
    axis=1, gamma_constraint=min_max_norm(),
    beta_constraint=min_max_norm())(in_vals)
in_locs = Input((poscount, ), name='locs', dtype='uint64')
embed_locs = Embedding(
    locidx.watermark, embedding_dim, input_length=poscount)(in_locs)
merged = concatenate([embed_locs, normd])
drop = Dropout(dropout_prob[0])(merged)
conv_list = []
for filtsz in filter_sizes:
    tmp = Conv1D(num_filters, filtsz, activation='relu')(drop)
    tmp = Flatten()(MaxPooling1D()(tmp))
    conv_list.append(tmp)
out = Dense(
    1, activation='sigmoid')(Dense(hidden_dims, activation='relu')(Dropout(
        dropout_prob[1])(concatenate(conv_list))))
ml = Model(inputs=[in_locs, in_vals], outputs=out)
ml.compile(Adam(lr=0.01), metrics=['acc'], loss=binary_crossentropy)
locs, vals, labels = rfutils.read_data(gl, poscount, locidx)


def fit(
        eps=int(sys.argv[1]) if len(sys.argv) > 1 else 1,
        # Large batches tend to cause NaNs in batch normalization.
        bsz=int(sys.argv[2]) if len(sys.argv) > 2 else 50):
    ml.fit([locs, vals], labels, batch_size=bsz, epochs=eps)


fit()
