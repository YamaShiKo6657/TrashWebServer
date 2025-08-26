project(test_main)

add_subdirectory(log)
add_subdirectory(threadpool)
add_subdirectory(buffer)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME}
PUBLIC 
${Log_Dir}
)
target_include_directories(${PROJECT_NAME}
PUBLIC 
${Pool_Dir}
)

target_link_libraries(${PROJECT_NAME}     
PUBLIC
log)
target_link_libraries(${PROJECT_NAME}     
PUBLIC
threadpool)