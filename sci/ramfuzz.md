# RamFuzz: A Framework for C++ Test Generation via Deep Learning
Dejan Mircevski

November 2017

## Abstract

We have developed a framework for generating unit tests of arbitrary C++ source and processing their results by deep-learning neural networks. We show that a network originally developed for text processing can learn to accurately predict which randomized unit tests will fail and which will succeed, based on the random values generated. This is a promising first step in teaching artificial intelligence what a software program does and how to test it.

## Introduction

Randomized testing is a family of automated procedures that exercise software with randomly generated inputs.  It has proven very effective at finding bugs that manual testing could not.  From fuzzers to unit-test generators, the practice of feeding random inputs to a piece of software to expose its flaws has become invaluable in raising the quality and security of widely used programs the public depends on.  In particular, randomized unit testing (where tests exercise individual methods instead of the whole program) is helpful in isolating where a bug occurs, making it easier to diagnose and fix.  Moreover, unit testing works to fortify the quality of small software components, increasing reusability of those components and with it the programmer's overall efficiency.

Most research in randomized unit-test generation is in languages other than C++, despite C++ accounting for a substantial fraction of all software, particularly if weighted by usage.  Essential programs like browsers, compilers, build systems, and cloud infrastructure are written in C++.  There is every reason to believe that randomized unit testing is just as valuable for C++ as it demonstrably is for other languages, yet there is far less support for it.  To help bring about more C++ tools for randomized unit-test generation, we have developed the [RamFuzz framework](https://github.com/dekimir/RamFuzz).  RamFuzz can create randomized input for unit tests, including random instances of user-defined C++ classes.  The input generation is logged, leaving a precise record of the test run.  The log can be used to replay the test run for debugging purposes.

Additionally, a set of logs can be fed to deep-learning algorithms via a convenient Python interface.  This allows application of artificial intelligence (AI) to the process, leading to some very interesting results.  It also provides a framework for further research into leveraging AI to automatically test C++ software.

In this paper, we give an overview of the RamFuzz code generator, runtime logger, and post-processing interface to Python tools for deep learning.  We then show how the framework allows a neural network to learn to discern failing-test logs from the successful ones.  Although this is not the same as generating useful unit tests, it is a promising step on that path and an illustration of the framework's overall capabilities.  More research is needed to attain automatic test generation, as we describe in the final section.  We argue that RamFuzz is a potent vehicle for conducting that future research.

## Generating and Logging Random Parameter Values

In this section we outline the main aspects of parameter-value generation.  Readers interested in the details of any particular case are referred to the RamFuzz [source code](https://github.com/dekimir/RamFuzz), available on GitHub.  The RamFuzz code generator does not currently handle all language constructs nor all semantic scenarios, but it does enough to be usable on real-life projects such as [Apache Mesos](http://mesos.apache.org) and [Google Protocol Buffers](https://developers.google.com/protocol-buffers/).  In the next section, we will present deep-learning results obtained by running RamFuzz tests on the Mesos class `Resources`.

The code generator parses the user's C++ source and generates test code that randomly exercises each public method at runtime, supplying random values for arguments.  If a parameter's type is numeric, we simply generate a random number within the boundaries of that type.  If the type is a class, we create an instance of it by invoking a random constructor; then we invoke a random series of instance methods whose parameter values we generate recursively.  In the end, this yields a random value of that class.  Other types (eg, pointers, references, enums, etc.) are handled by reducing them to previous cases and post-processing the results.

All parameter-value creation therefore ultimately reduces to numeric values: a random method is selected by generating a random integer between 1 and the total number of methods; a random sequence is produced by generating a random non-negative integer N denoting the sequence size, then recursively generating N elements; and so on.  The RamFuzz runtime library logs the generated numeric values, and it allows the user to replay the log on the next test run.  A failed test can thus be repeated exactly to see what caused the failure.

Obviously, feeding random values to methods under test will not always result in valid tests.  Most methods impose various constraints on the parameters they expect.  But if the process guarantees *a reasonable probability* of sometimes generating valid parameter values, that is enough to produce useful input to deep-learning algorithms.  The primary goal of random-value generation is to feed deep learning something useful.  Running random tests again and again is easy, so the process can generate unlimited amounts of training and validation data.

RamFuzz-generated code applies a variety of techniques to maximize the chances of hitting upon valid parameter values. For example:

- RamFuzz occasionally reuses a previously generated value.  This avoids failure when a method requires the same parameter that was previously given to it or another method.  Without the reuse mechanism, it would not be possible to run a valid test on methods that expect it -- the probability of a valid run would be zero.  The reuse happens with a certain small probability every time the test code is asked to generate a value.

- RamFuzz keeps track of all available inheritance relationships.  When asked to generate a random instance of a base class, the RamFuzz runtime can construct a randomly chosen subclass.  This ensures the possibility of success where a method expects a subclass to be passed.  It also maximizes the test coverage by testing all possible variations accepted by a method's signature.

- RamFuzz generates concrete implementations of all abstract classes.  This allows for testing of methods that take abstract-class parameters.  Whenever a concrete implementation's method must return a value, that value is randomly generated just like parameter values discussed so far.

- RamFuzz even supports generating random values of class-template instantiations, just like any other concrete types.

## Applying Deep Learning to the Logs

Due to the wealth of deep-learning libraries available in Python, the best way to support the processing of RamFuzz logs is to allow Python code to read them.  RamFuzz logs are written in binary format to avoid the imprecision of text format (where reading a floating-point value from text may result in a slightly different value from the original, altering the test run).  The logged values are not all of the same C++ type -- there are at least 13 different C++ numerical types of varying width.  Most of these C++ types don't map directly to native Python types, so Python cannot read the logs natively.  But RamFuzz includes a [custom Python module](https://github.com/dekimir/RamFuzz/tree/master/pymod) implemented in C++ that reads the logs and feeds values back to Python as Python-native types.

To enhance the quality of deep-learning analysis, we log not just the values themselves but also where in the program they were generated.  This enriches the input information and allows deep-learning algorithms to take into account what happens at different program locations.  The RamFuzz runtime library calculates the program location as a hash of all program-counter registers currently on the call stack.  This somewhat arbitrary definition works well enough to achieve high classification accuracy, but it is not hardcoded; a different definition could be substituted in without changing the rest of the framework.

### Experimental Setup

We now describe an experiment where we generate 10,000 logs of the same randomized test run and use them as a training corpus for a neural network aiming to discern failing from successful runs.  We started with the Mesos source code and applied the RamFuzz code generator to the header file [`include/mesos/resources.hpp`](https://github.com/apache/mesos/blob/master/include/mesos/resources.hpp).  We then manually implemented a [new Mesos unit test](https://github.com/dekimir/mesos/blob/ramfuzz/src/tests/ramfuzz/http_tests.cpp) leveraging the generated code.  The test is quite simple: it just creates a random `Resources` object.  If this creation is successful, the test passes.  If, on the other hand, the creation process crashes or fails an internal assertion due to invalid random parameter values, the test fails.  While this is a shallow test, it is cheap to make (even by software), and it produces good training data for deep learning.  The test's technical details are publicly available in our [Mesos fork](https://github.com/dekimir/mesos/tree/ramfuzz/ramfuzz) on GitHub.

We extended Mesos makefiles to build the new unit test and produce an executable that includes the RamFuzz-generated test code and runtime library.  RamFuzz provides a [script](https://github.com/dekimir/RamFuzz/blob/master/ai/gencorp.py) to run this executable many times and save all run logs, classifying them as success or failure based on the executable's exit status.  This prepares a training corpus for the deep-learning model.  We performed 10,000 runs of the RamFuzz-powered test.  About 56% of the runs were failures, 44% successes (we repeated the experiment several times, both on Linux and Mac OS, with consistent results).

### Preparing Data

RamFuzz provides a [Python library](https://github.com/dekimir/RamFuzz/blob/master/ai/rfutils.py) to parse the logs and load the data into a NumPy matrix.  For network-training purposes, the data is organized as follows:

- Program locations are indexed with dense integers, similar to natural-language words in the model's original purpose.  These are suitable for feeding into a Keras embedding layer.  Indices start at 1.

- The network model has two inputs: an array of values from a single log sorted by their position and an array of location indices corresponding to these values.  Arrays are padded by zeros to match the length of the longest log, so they are all of the same length.  Since location indices start at 1, the padding elements are distinct from all valid locations.

- All values are converted to a single type, so they can be packed into a NumPy array.  The type is `np.float64`, to ensure the maximum possible range for receiving logged values.  Location indices are of type `np.uint64` for the same reasons.

### Training

The network model is adapted from a public Keras [implementation](https://github.com/alexander-rakhlin/CNN-for-Sentence-Classification-in-Keras) of Kim's convolutional network for sentence classification [1] -- the idea is that a RamFuzz log is analogous to a sentence describing the test run in a very particular language.  This idea seems to have merit, as the model in question performs much better than other models we tried.  The model's output is a single number, indicating the confidence that the input comes from a successful test run.

Because the ranges of generated values can be extremely wide, the original model's training was not effective: its weights would reach extreme values or NaNs, resulting in low accuracy.  We introduced a batch-normalization layer to reduce the values before any further processing.  We also had to significantly lower the batch size to avoid NaNs in batch-normalizer's mean calculation.  The resulting model is also [available on GitHub](https://github.com/dekimir/RamFuzz/blob/master/ai/sample-model1.py).

With the above adaptations, the training achieved over 99% prediction accuracy within five epochs.  These results were consistent across multiple runs on both Linux and Mac OS.  In one Linux run, the training actually achieved 100% accuracy.

### Validation

After training the model, we validated it against a separate set of test runs.  In various experiments, we performed between 2,000 and 4,500 additional test runs and checked the model's prediction for those logs.  Any previously unseen program locations in the new logs were simply skipped, ignoring the corresponding values.  In all cases, the prediction accuracy was over 90%.

## Conclusions

The RamFuzz code generator makes it easy to create randomized unit tests in C++, and its AI tools make it easy to generate unlimited quantities of training data and feed it to deep-learning models.  RamFuzz will therefore enable more effective research in several important directions.

The surprising accuracy of deep learning at predicting test failures suggests that AI can play a major role in automated test generation.  Consider that the trained deep-learning model in fact represents AI's knowledge of the tested software's specification.  In time, we may be able to translate such specifications into human language, check them for code quality, or even compare specifications of different programs (or different versions of the same program).  More ambitiously, we may be able to use the trained model's knowledge to constrain the runtime generation of random values in a way that yields only valid tests.  This would give us the first autonomous system for testing existing C++ software.

One notable shortcoming of RamFuzz currently is that it falls short of generating entire unit tests.  It generates test inputs but not test assertions.  In fact, this is common with many other tools for automatic test generation, and it stems from the simple fact that the process is missing another input: the source code's specification.  Without a specification, it is simply impossible to devise tests that verify whether the code conforms to it.  Nevertheless, the experience of prior tools for autonomous test generation is that even tests with trivial or missing assertions can be useful in finding bugs [2, 3].  For instance, Microsoft found 30 new bugs using a .NET test generator on production code that had previously undergone 200 person-years of testing and had 100% code coverage by handcrafted tests [7].  The bugs stemmed from unanticipated sequences of method invocations, and they ranged from memory corruption to missing resource-file entries.  The tool used only the most trivial test assertions (eg, no crashing) in the generated unit tests.

## Future Work

While the experiment described above gives encouraging results, it involves only one simple test.  The first outstanding task, then, is to explore what happens with a wider variety of tests.  It is possible that the model is overfitting to this particular test case (though not to a specific set of test runs -- the results hold across multiple different sets).  Also, training might get harder when there is imbalance between the success and failure counts, unlike with this test.  If so, we will need to tweak the RamFuzz code generator to ensure more suitable success/failure ratios.

Additionally, it would be interesting to examine the weights of the model trained to accurately predict test failures.  There may be correlation between certain features of the weights and software behaviour such as bugs, performance, or readability.  We could also compare the weights of two models trained on different runs of the same test, or even on different implementations of the class under test.  These various comparisons and classifications can be done either by humans or by another AI.

An intriguing idea is to leverage the weights of the trained model to provide feedback to the RamFuzz runtime, letting it restrict the generated values to only valid tests.  We have demonstrated this to be possible with a [model](https://github.com/dekimir/RamFuzz/blob/master/ai/sample-model2.py) much simpler than the one used in this paper, trained on contrived test runs of fixed length.  Once the model is trained, it can be viewed as a set of inequalities on its inputs; when the inequalities are satisfied, the test is predicted a success.  These inequalities can then drive the runtime value generation on subsequent test runs.  Whenever the RamFuzz runtime needs to generate a random numerical value, it can plug the values generated so far into the system of inequalities and apply the Fourier–Motzkin elimination to arrive at the valid range for the value currently being generated.  In our contrived test runs, we were able to train the model to 98% accuracy, feed its weights back to the random-value generation process, and achieve 93% success rate on subsequent test runs.  Unfortunately, the results did not carry over into the real test runs, where the simple model in question could not achieve accuracy over 56%.  The model we use here, which reliably achieves over 99% accuracy, does not readily translate into a set of inequalities.  But if we could find a model both accurate and reversible, we would have a complete system for generating valid randomized tests.

Finally, we need a more stable way of capturing program location.  The current method is stable across test runs as long as the executable does not change, but if the source is modified and rebuilt, all the location hashes change.  This is not conducive to extending conclusions across different versions of the source, even if the differences are insignificant.

## Related Work

Several tools exist for generating randomized unit tests in languages other than C++.  For instance, Microsoft Visual Studio contains a tool named IntelliTest, which generates randomized unit tests for C# classes.  Unfortunately, IntelliTest can autonomously generate only inputs of simple types.  As its manual [states](https://docs.microsoft.com/en-us/visualstudio/test/intellitest-manual/input-generation#objects), IntelliTest "needs help to create objects and bring them into interesting states".  In contrast, RamFuzz only needs user help when a class has no public constructors at all.

[Randoop](https://randoop.github.io/randoop/) is a randomized unit-test generator for Java and .NET classes.  It generates test inputs in much the same way as RamFuzz, by creating random sequences of method calls [2]. [AgitarOne](http://www.agitar.com/solutions/products/automated_junit_generation.html) is a tool for generating Java unit tests based on the *agitation* technique [6].  It uses AI models for predicting and explaining test coverage [5], but not for processing the test runs themselves.  [EvoSuite](http://evosuite.org) and [eToc](http://star.fbk.eu/etoc/) use genetic algorithms to evolve Java test suites from randomly selected seeds in order to maximize code coverage [3, 4].  Currently, none of these tools provide an interface to easily feed the generated test cases to existing deep-learning libraries.

Fuzzers like [AFL](http://lcamtuf.coredump.cx/afl/) and [libFuzzer](https://llvm.org/docs/LibFuzzer.html) can apply random inputs to programs in any language, including C++.  Like EvoSuite and eToc, fuzzers mutate the inputs to maximize the code coverage.  But they only work on the whole program, not at the individual-method level; and they require the program to read all its input from a single file.  While it may be possible to create unit tests driven by an input file, it is doubtful that the file's format would lend itself well to the common [mutation techniques](http://lcamtuf.blogspot.com/2014/08/binary-fuzzing-strategies-what-works.html) used by fuzzers.  (In fact, RamFuzz began its life as an attempt to accomplish exactly that: provide an input file for a fuzzer to drive unit testing; the name RamFuzz is derived from "parameter fuzzing".  Along the way, we realized that we could not effectively apply common fuzzers to RamFuzz logs.)  Fuzzing is, therefore, not well suited for randomized unit testing and remains best used to detect crashes of file-processing programs.

## References

[1] [Convolutional Neural Networks for Sentence Classification](https://arxiv.org/pdf/1408.5882v2.pdf), Yoon Kim; arXiv:1408.5882v2, 2014

[2] [Feedback-directed Random Test Generation](https://homes.cs.washington.edu/~mernst/pubs/feedback-testgen-icse2007.pdf), Carlos Pacheco, Shuvendu K. Lahiri, Michael D. Ernst, and Thomas Ball; ICSE '07: Proceedings of the 29th International Conference on Software Engineering, (Minneapolis, MN, USA), 2007

[3] [EvoSuite: Automatic test suite generation for object-oriented software]( https://www.researchgate.net/publication/221560749), Gordon Fraser and Andrea Arcuri; SIGSOFT/FSE 2011: Proceedings of the 19th ACM SIGSOFT Symposium on Foundations of Software Engineering, 2011

[4] [Evolutionary Testing of Classes](https://pdfs.semanticscholar.org/4f6b/cd2bffc305b8ba56933af332d9da82daeebc.pdf), Paolo Tonella; ISSTA '04: Proceedings of the 2004 ACM SIGSOFT International Symposium on Software Testing and Analysis, 2004

[5] [Predicting and Explaining Automatic Testing Tool Effectiveness](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.218.4141&rep=rep1&type=pdf), Brett Daniel and Marat Boshernitsan; ASE 2008: 23rd IEEE/ACM International Conference on Automated Software Engineering, 2008

[6] [From Daikon to Agitator: Lessons and Challenges in Building a Commercial Tool For Developer Testing](https://web.stanford.edu/class/archive/cs/cs295/cs295.1086/papers/ind6-boshernitsan.pdf), Marat Boshernitsan, Roongko Doong, and Alberto Savoia; ISSTA '06: Proceedings of the 2006 International Symposium on Software Testing and Analysis, 2006

[7] [Finding Errors in .NET with Feedback-directed Random Testing](http://people.csail.mit.edu/cpacheco/publications/pacheco_finding_errs.pdf), Carlos Pacheco, Shuvendu K. Lahiri, and Thomas Ball; ISSTA 2008: International Symposium on Software Testing and Analysis, 2008
