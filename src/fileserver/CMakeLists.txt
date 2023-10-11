# 定义一个SRC_LIST变量，包含了该目录下所有的源文件
aux_source_directory(./src SRC_LIST)
aux_source_directory(/project/wsm/server/public PUBLIC_LIST)

# 指定生成可执行文件
add_executable(FileServer ${SRC_LIST} ${PUBLIC_LIST})
# 指定可执行文件链接时需要依赖的库文件
target_link_libraries(FileServer pthread)