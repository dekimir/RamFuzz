#!/Users/dejan2/anaconda2/bin/python

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

import copy
import numpy as np
import sys
from keras import backend as K
from keras.layers import Dense, Input
from keras.layers.merge import multiply
from keras.layers.normalization import BatchNormalization
from keras.metrics import mse
from keras.models import Model
from keras.optimizers import Adam
from theano import tensor as T


maxint = 99999
minint = -99999
datrn = []
labtrn = []
for _ in xrange(50000):
    r = np.random.randint(minint, maxint, 4)
    datrn.append(r)
    labtrn.append(r[0] <= r[2] and r[1] <= r[3])
datrn = np.array(datrn)
labtrn = np.array(labtrn)

dense_count = int(sys.argv[1]) if len(sys.argv) > 1 else 5

inp = Input(shape=(4, ))
bnorm = BatchNormalization(input_shape=(4, ))(inp)
dense_list = []
for i in range(dense_count):
    dense_list.append(Dense(1, activation='sigmoid')(bnorm))
mult = multiply(dense_list)
ml = Model(inputs=inp, outputs=mult)
ml.compile(Adam(lr=0.01), metrics=['acc'], loss=mse)
ml.fit(datrn, labtrn, batch_size=1000, epochs=50, verbose=0)

threshold = 0.2
predictions = ml.predict(datrn)[:, 0]
errx = ((predictions > threshold) != labtrn).nonzero()[0]
print 1. - len(errx) / float(len(datrn))


def discrep(i):
    return predictions[i], labtrn[i]


def fitmore(epochs=5):
    ml.fit(datrn, labtrn, batch_size=1000, epochs=epochs)
    predictions = ml.predict(datrn)[:, 0]
    errx = ((predictions > threshold) != labtrn).nonzero()[0]
    print 1. - len(errx) / float(len(datrn))


def layer_output(layer_tensor, corpus):
    f = K.function([inp, K.learning_phase()], layer_tensor)
    return f([corpus] + [0.])[:, 0]


def despite_layer(layer_tensor):
    return labtrn * (layer_output(layer_tensor, datrn) == 0)


def despite_layer_indices(layer_tensor):
    return despite_layer(layer_tensor).nonzero()[0]


def effective_weights(lix):
    batchnorm_weights = ml.layers[1].get_weights()
    epsilon = ml.layers[1].epsilon
    gamma = batchnorm_weights[0]
    beta = batchnorm_weights[1]
    mean = batchnorm_weights[2]
    varce = batchnorm_weights[3]
    dense_weights = ml.layers[lix].get_weights()
    kernel = dense_weights[0][:, 0]
    bias = dense_weights[1][0]
    F = gamma / np.sqrt(varce + epsilon)
    eff_bias = bias + np.dot(kernel, beta - mean * F)
    return [kernel * F, eff_bias]


ineqs = []
for i in range(dense_count):
    efw = effective_weights(i + 2)
    ineqs.append(np.append(efw[0], efw[1]))
    print "Weights%d: %r" % (i, efw)
    print 'Tests succeeded despite dens%d fail: %d' % (
        i, len(despite_layer_indices(dense_list[i])))
for x in range(4):
    upper = np.array([0., 0., 0., 0., maxint])
    upper[x] = -1.
    ineqs.append(upper)
    lower = np.array([0., 0., 0., 0., -minint])
    lower[x] = 1.
    ineqs.append(lower)


def fomo_step(ineqs):
    pos = []
    neg = []
    res = []
    for w in ineqs:
        fac = w[1]
        w = np.delete(w, 1)
        if fac == 0:
            res.append(w)
            continue
        xformd = -w / fac
        if fac > 0:
            pos.append(xformd)
        elif fac < 0:
            neg.append(xformd)
        else:
            res.append(xformd)
    for p in pos:
        for n in neg:
            res.append(n - p)
    return res


def bounds(ineqs):
    lo = minint
    hi = maxint
    if len(ineqs) == 0:
        return (lo, hi)
    shape0 = ineqs[0].shape
    assert(len(shape0) == 1)
    if shape0[0] == 2:
        for (a, b) in ineqs:
            if a > 0:
                lo = max(lo, -b/a)
            elif a < 0:
                hi = min(hi, -b/a)
            else:
                assert(b > 0)
        return (lo, hi)
    else:
        return bounds(fomo_step(ineqs))


def gen(ineqs):
    try:
        return np.random.randint(*bounds(ineqs))
    except ValueError:
        return 0.


def subst(val, ineqs):
    res = []
    for m in ineqs:
        sh = m.shape
        assert(len(sh) == 1 and sh[0] > 1)
        r = copy.deepcopy(m)
        r[-1] = r[-1] + r[0] * val
        res.append(np.delete(r, 0))
    return res


def gen4(ineqs):
    ram0 = gen(ineqs)
    ie1 = subst(ram0, ineqs)
    ram1 = gen(ie1)
    ie2 = subst(ram1, ie1)
    ram2 = gen(ie2)
    ie3 = subst(ram2, ie2)
    ram3 = gen(ie3)
    return [ram0, ram1, ram2, ram3]


def badfrac(ineqs, iteration_count=10000):
    cnt = 0
    for _ in xrange(iteration_count):
        rams = gen4(ineqs)
        cnt += (rams[0] > rams[2] or rams[1] > rams[3])
    return float(cnt)/iteration_count


print badfrac(ineqs, 50000)
idl = [np.array([-1., 0., 1., 0., 0.]), np.array([0., -1., 0., 1., 0.])]
print badfrac(idl, 100)
