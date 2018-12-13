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
"""Save a test torch.jut.script.model, for C++ tests to load."""

from torch import jit, nn, randn


class Net(nn.Module):
    def __init__(self):
        super(Net, self).__init__()
        self.fc1 = nn.Linear(10, 2)

    def forward(self, x):
        return nn.functional.softmax(self.fc1(x), dim=0)


jit.trace(Net(), randn(10)).save('delete_me.pt')
