cmake_minimum_required(VERSION 3.8)
project(data_proxy)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



###################################################################
#第一次编译打开Build_Third_Party 编译第三方库 之后关闭该选项

set(Build_Third_Party OFF)
set(CMAKE_BUILD_TYPE_COPY)
if (Build_Third_Party)
    message("编译第三方库")
    set(CMAKE_BUILD_TYPE_COPY ${CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE Release)
    include_directories(${CMAKE_SOURCE_DIR}/third_party)

    set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/third_party/lib)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/third_party/bin)

    #set_target_properties(cjson PROPERTIES

    add_subdirectory(third_party/protobuf)
    add_subdirectory(third_party/cjson)
    add_subdirectory(third_party/mosquitto)
    add_subdirectory(third_party/fmt)
    add_subdirectory(third_party/gflags)
endif()

if (Build_Third_Party)
    set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE_COPY})
endif()

######################################################################

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()


include_directories(${CMAKE_SOURCE_DIR}/third_party/protobuf/src)
include_directories(${CMAKE_SOURCE_DIR}/third_party/protobuf/third_party/abseil-cpp)
include_directories(${CMAKE_SOURCE_DIR}/third_party/mosquitto/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/mosquitto/lib/cpp)
include_directories(${CMAKE_SOURCE_DIR}/third_party/fmt/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/asio-1.30.2/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/include)

# find dependencies
find_package(ament_cmake REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/logic SRC)

add_executable(data_proxy ${SRC})
target_include_directories(data_proxy PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
#target_compile_features(data_proxy PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17

target_include_directories(data_proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/data_proxy)
target_link_directories(data_proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib)

target_link_libraries(data_proxy
	fmt
  mosquittopp
)
ament_target_dependencies(data_proxy rclcpp std_msgs)

install(TARGETS data_proxy
  DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
