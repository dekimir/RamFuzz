#!/usr/bin/env python

# Copyright 2016-2018 The RamFuzz contributors. All rights reserved.
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
"""A sample Keras model trainable on a set of fuzzlogs.  The model is adapted
from Alexander Rakhlin's sample implementation of NLP CNN:
https://github.com/alexander-rakhlin/CNN-for-Sentence-Classification-in-Keras.

Usage: $0 [epochs] [prediction_threshold]
Defaults: epochs=1, prediction_threshold=0.7

Expects a train/ subdirectory containing fuzzlogs whose filenames indicate
whether the run was a success or failure.  Filenames ending in `.0` are success
fuzzlogs, while all other files are failure fuzzlogs.  (This makes it easy to
generate fuzzlogs via shell commands like `./runtest; mv fuzzlog
train/$((counter++)).$?`.)

"""

from keras.constraints import min_max_norm
from keras.layers import BatchNormalization, Dense, Dropout, Conv1D
from keras.layers import Embedding, Flatten, Input, MaxPooling1D
from keras.layers.merge import concatenate
from keras.metrics import binary_crossentropy
from keras.models import Model
from keras.optimizers import Adam
import glob
import keras.backend as K
import numpy as np
import os.path
import rfutils
import sys

sys.setrecursionlimit(9999)

exetree = rfutils.node()
files = glob.glob(os.path.join('train', '*'))
for f in files:
    exetree.add(rfutils.open_and_logparse(f), f.endswith('.0'))

poscount = exetree.depth()
locidx = exetree.locidx()

K.set_floatx('float64')


def log_to_locs_vals(log, locidx, poscount):
    nlocs = np.zeros(poscount, np.uint64)
    nvals = np.zeros((poscount, 1), np.float64)
    for p, (v, l) in enumerate(log):
        if p >= poscount:
            break
        idx = locidx.get_index(l)
        if idx:
            nlocs[p] = idx
            nvals[p] = v
    return nlocs, nvals


def get_training_data(tree_root, locidx, poscount):
    """Builds input data from a files list."""
    locs = []  # One element per node; each is a list of location indexes.
    vals = []  # One element per node; each is a parallel list of values.
    labels = []  # One element per node: true iff node.reaches_success.
    for n in tree_root.preorder_dfs():
        # TODO: use log_to_locs_vals.
        nlocs, nvals = log_to_locs_vals(n.logseq(), locidx, poscount)
        locs.append(nlocs)
        vals.append(nvals)
        labels.append(n.reaches_success)
    return np.array(locs), np.array(vals), np.array(labels)


locs, vals, labels = get_training_data(exetree, locidx, poscount)
vals[vals > 1e5] = 1e5  # Clip to avoid NaNs in batch normalization.

embedding_dim = 40
filter_sizes = (3, 8)
num_filters = 10
dropout_prob = (0.1, 0.2)
hidden_dims = 50

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


def fit(eps, bsz=200):
    ml.fit([locs, vals], labels, batch_size=bsz, epochs=eps)


def layerfun(i):
    """Returns a backend function calculating the output of i-th layer."""
    return K.function(
        [ml.layers[0].input, ml.layers[1].input,
         K.learning_phase()], [ml.layers[i].output])


def layer_output(l, i):
    """Returns the output of layer l on input i."""
    return layerfun(l)([locs[i:i + 1], vals[i:i + 1], 0])[0]


def predict(log):
    """Returns the output of ml on fuzzlog (a list of pairs)."""
    x, y = log_to_locs_vals(log, locidx, poscount)
    return ml.predict([np.array([x]), np.array([y])])[0, 0]


def predict_vs_label(n):
    return predict(n.logseq()), n.reaches_success


def convo(layer, input, i):
    """Convolves the Conv1D layer's weights with input at position i.

    input = array of shape (n,m) where n is the length of layer's output and m
    is the width of layer's weights.

    i = integer between 0 and n-1, indicating input index at which to begin
    convolution (and also the index of layer's output where the convolution
    result will appear when the model is run)

    """
    wts = layer.get_weights()[0][::-1]
    bias = layer.get_weights()[1]
    return np.sum([np.dot(input[i + j], w) for j, w in enumerate(wts)]) + bias


def convo_elements(layer_input, weights, offset):
    """Prints individual convolution elements at given offset.

    Returns their sum.

    """
    sum = 0
    for i in range(0, 8):
        e = np.dot(weights[i], layer_input[0, offset + i])
        print ' %20.18f #o%dw%d' % (e, offset + i, i)
        sum += e
    return sum


def validate(tree_root, prediction_threshold):
    """Prints results of ml's validation over a tree.

    Validation is specific to the intended use of ml as a go/no-go indicator to
    RamFuzz runtime when it attempts to enter a state corresponding to some
    node.  The intent is for RamFuzz to enter that state only if ml predicts
    the node may reach success; otherwise, RamFuzz is to generate another value
    leading to another state.

    To that end, ml's prediction of a node matters only if all ancestors of
    that node were predicted correctly.  Otherwise, either the node will not
    normally be reached (if a reaches_success==True ancestor was mispredicted)
    or ml has failed to prevent every failure under the mispredicted
    reaches_success==False ancestor.

    """
    mispredicted_success_count = 0
    unreachable_success_leaves_count = 0
    mispredicted_failure_count = 0
    worklist = [(tree_root, [])]  # Each element is a (node, logseq) pair.
    while worklist:
        n, log = worklist.pop()
        if n.reaches_success == (predict(log) >= prediction_threshold):
            for e in n.edges:
                worklist.append((e[1], log + [(e[0], n.loc)]))
        elif n.reaches_success:
            mispredicted_success_count += 1
            for d in n.preorder_dfs():
                if d.terminal == 'success':
                    unreachable_success_leaves_count += 1
        else:
            mispredicted_failure_count += 1
    print 'Missed %d MAYWIN nodes, cutting off %d WIN leaves.' % (
        mispredicted_success_count, unreachable_success_leaves_count)
    print 'Missed %d NOWIN nodes.' % mispredicted_failure_count


def predict_with_tree(model_tree, log):
    """Like predict(), but uses model_tree instead of ml.

    Returns False if log (a list of value/location pairs) corresponds to an
    existing node in model_tree and that node does not reach success.
    Otherwise, returns True.

    """
    if log == []:
        return model_tree.reaches_success
    val, loc = log[0]
    assert (loc == model_tree.loc)
    for e in model_tree.edges:
        if e[0] == val:
            return predict_with_tree(e[1], log[1:])
    return True


def validate_with_tree(model_tree, validation_tree):
    """Like validate(), but uses model_tree instead of ml."""
    mispredicted_success_count = 0
    unreachable_success_leaves_count = 0
    worklist = [(validation_tree, [])]  # Each element = (node, logseq) pair.
    while worklist:
        n, log = worklist.pop()
        if n.reaches_success == predict_with_tree(
                model_tree, log) or not n.reaches_success:
            for e in n.edges:
                worklist.append((e[1], log + [(e[0], n.loc)]))
        else:
            mispredicted_success_count += 1
            for d in n.preorder_dfs():
                if d.terminal == 'success':
                    unreachable_success_leaves_count += 1
    print 'Missed %d MAYWIN nodes, cutting off %d WIN leaves.' % (
        mispredicted_success_count, unreachable_success_leaves_count)


fit(eps=int(sys.argv[1]) if len(sys.argv) > 1 else 1)
prediction_threshold = float(sys.argv[2]) if len(sys.argv) > 2 else 0.7
print 'Training data:'
validate(exetree, prediction_threshold)

glval = glob.glob(os.path.join('valn', '*'))
if glval:
    etv = rfutils.node()
    for f in glval:
        etv.add(rfutils.open_and_logparse(f), f.endswith('.0'))
    print '\nValidation by model:'
    validate(etv, prediction_threshold)
    print '\nValidation by history:'
    validate_with_tree(exetree, etv)
