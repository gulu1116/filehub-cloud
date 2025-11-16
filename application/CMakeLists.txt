add_subdirectory(filehub)

include_directories(
  ${PROJECT_SOURCE_DIR}                # 全局根目录
  ${PROJECT_SOURCE_DIR}/muduo/include  # muduo头文件目录
  ${CMAKE_CURRENT_SOURCE_DIR}          # application目录（方便内部引用）
  ${CMAKE_CURRENT_SOURCE_DIR}/base     # 本地base目录
  ${CMAKE_CURRENT_SOURCE_DIR}/api      # 本地api目录
  /usr/include/jsoncpp                 # jsoncpp目录
)
