cmake_minimum_required(VERSION 3.15)
project(PRReviewService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/models)

include_directories(/opt/homebrew/opt/libpq/include)

set(SOURCES
    src/main.cpp
)

add_executable(pr_review_service ${SOURCES})

find_package(CURL REQUIRED)

add_executable(integration_test tests/integration_test.cpp)
target_link_libraries(integration_test ${CURL_LIBRARIES} pthread)

add_test(NAME IntegrationTests COMMAND integration_test)

target_link_libraries(pr_review_service 
    ${PostgreSQL_LIBRARIES}
    pthread
)

target_link_libraries(pr_review_service 
    /opt/homebrew/opt/libpq/lib/libpq.dylib
    pthread
)