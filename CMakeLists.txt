cmake_minimum_required(VERSION 3.8)
project(topsbot_cv)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(tb_img_msgs REQUIRED)

if(CMAKE_SYSROOT)
  set(TOPSBOT_TACV_ROOT "${CMAKE_SYSROOT}/usr/local")
else()
  set(TOPSBOT_TACV_ROOT "/usr/local")
endif()

set(TOPSBOT_TACV_INCLUDE_DIR "${TOPSBOT_TACV_ROOT}/include")
set(TOPSBOT_TACV_LIB_DIR "${TOPSBOT_TACV_ROOT}/lib")

add_library(${PROJECT_NAME}_lib SHARED
  src/dmabuf_allocator.cpp
  src/ta_image.cpp
  src/image_source.cpp
  src/imgproc.cpp
  src/format_convert.cpp
  src/nv12_fill.cpp
  src/ffmpeg_decode_util.cpp
  src/pipeline_executor.cpp
)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${TOPSBOT_TACV_INCLUDE_DIR}
)

target_link_directories(${PROJECT_NAME}_lib PUBLIC ${TOPSBOT_TACV_LIB_DIR})
target_link_options(${PROJECT_NAME}_lib PUBLIC "-Wl,-rpath-link,${TOPSBOT_TACV_LIB_DIR}")

target_link_libraries(${PROJECT_NAME}_lib
  tacocv
  taco_jpegdec
  taco_ppvdec
  enc24
  dmabufheap
  tacosys
)

ament_target_dependencies(${PROJECT_NAME}_lib
  sensor_msgs
)

add_library(${PROJECT_NAME}_node SHARED
  src/image_processor_node.cpp
  src/node_params.cpp
)

target_include_directories(${PROJECT_NAME}_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}_node
  ${PROJECT_NAME}_lib
)

ament_target_dependencies(${PROJECT_NAME}_node
  rclcpp
  rclcpp_components
  sensor_msgs
  tb_img_msgs
)

rclcpp_components_register_nodes(${PROJECT_NAME}_node "topsbot_cv::ImageProcessorNode")

add_executable(topsbot_cv_image_processor src/image_processor_main.cpp)
target_link_libraries(topsbot_cv_image_processor ${PROJECT_NAME}_node)
ament_target_dependencies(topsbot_cv_image_processor rclcpp)

add_executable(topsbot_cv_file_cli src/file_cli_main.cpp)
target_link_libraries(topsbot_cv_file_cli ${PROJECT_NAME}_lib)

install(TARGETS topsbot_cv_image_processor topsbot_cv_file_cli
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}_lib ${PROJECT_NAME}_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

set(TOPSBOT_CV_TEST_IMAGES
  imgs/test.jpg
)
install(FILES ${TOPSBOT_CV_TEST_IMAGES}
  DESTINATION share/${PROJECT_NAME}/imgs
)

install(FILES QUICKSTART.md
  DESTINATION share/${PROJECT_NAME}
)

install(PROGRAMS scripts/run_usb_cam_pipeline_regression.sh
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  set(TOPSBOT_CV_GTESTS
    test_jpeg_dec
    test_pipeline
    test_convert_padding
    test_nv12_fill
    test_image_source_nv12
    test_image_source_bgr8
    test_image_source_mjpeg
    test_csc
  )

  foreach(_test_name ${TOPSBOT_CV_GTESTS})
    ament_add_gtest(${_test_name} test/${_test_name}.cpp)
    if(TARGET ${_test_name})
      target_include_directories(${_test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${TOPSBOT_TACV_INCLUDE_DIR}
      )
      target_link_libraries(${_test_name} ${PROJECT_NAME}_lib)
      ament_target_dependencies(${_test_name} rclcpp sensor_msgs ament_index_cpp)
      install(TARGETS ${_test_name}
        RUNTIME DESTINATION lib/${PROJECT_NAME}
      )
    endif()
  endforeach()
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_lib)
ament_export_dependencies(
  rclcpp
  sensor_msgs
  tb_img_msgs
)

ament_package()
