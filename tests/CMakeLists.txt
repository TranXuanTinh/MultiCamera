# Test Sources
set(COMMON_TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/event_bus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/preprocessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/postprocessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/camera_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/inference_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/kalman_filter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/hungarian.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/tracker_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service/log_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/app/pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/app/rule_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/app/rules/zone_rule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/app/rules/counting_rule.cpp
)

add_executable(test_pipeline test_pipeline.cpp ${COMMON_TEST_SRCS})
target_link_libraries(test_pipeline PRIVATE ${OpenCV_LIBS} Threads::Threads)
add_test(NAME test_pipeline COMMAND test_pipeline)

add_executable(test_tracker test_tracker.cpp ${COMMON_TEST_SRCS})
target_link_libraries(test_tracker PRIVATE ${OpenCV_LIBS} Threads::Threads)
add_test(NAME test_tracker COMMAND test_tracker)

add_executable(test_rule_engine test_rule_engine.cpp ${COMMON_TEST_SRCS})
target_link_libraries(test_rule_engine PRIVATE ${OpenCV_LIBS} Threads::Threads)
add_test(NAME test_rule_engine COMMAND test_rule_engine)

