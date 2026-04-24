function(_sightread_add_fuzz_target target_name)
  add_executable(${target_name} ${ARGN})
  target_compile_options(${target_name} PRIVATE -fsanitize=address,fuzzer)
  target_link_libraries(
    ${target_name}
    PRIVATE -fsanitize=address,fuzzer sightread
  )
  _sightread_set_minimum_cpp_standard(${target_name})
  _sightread_add_warnings(${target_name})
endfunction()

_sightread_add_fuzz_target(chart_fuzzer chart_fuzzer.cpp)
_sightread_add_fuzz_target(midi_fuzzer midi_fuzzer.cpp)
_sightread_add_fuzz_target(qbmidi_fuzzer qbmidi_fuzzer.cpp)
_sightread_add_fuzz_target(ini_fuzzer ini_fuzzer.cpp)
