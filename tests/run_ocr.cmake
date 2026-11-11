# Run hebocr on IMAGE and compare its output with EXPECTED (ignoring
# line-ending differences, so the same files work on Windows and Linux).
execute_process(
  COMMAND ${HEBOCR} -i ${IMAGE} -o ${OUTPUT}
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "hebocr exited with ${rc} on ${IMAGE}")
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${OUTPUT} ${EXPECTED}
  RESULT_VARIABLE diff)
if(NOT diff EQUAL 0)
  message(FATAL_ERROR "Output differs from ${EXPECTED}\n  (see ${OUTPUT})")
endif()
