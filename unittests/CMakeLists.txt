add_unittest(check-ramfuzz
  RamFuzzTests InheritanceTest.cpp UtilTest.cpp RangeTrackerTest.cpp)
target_link_libraries(RamFuzzTests clangRamFuzz)
