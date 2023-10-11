# 获取源文件夹中的所有 C++ 文件
CPP_FILES := $(wildcard *.cpp)

# 将源文件列表转换为可执行文件列表
EXEC_FILES := $(patsubst %.cpp,%,$(CPP_FILES))

# 默认目标
all: $(EXEC_FILES)

# 用于编译每个可执行文件的目标规则
# 可执行文件的名称是对应的 cpp 文件名
%: %.cpp
	g++ $< -o $@ -pthread

# 清理目标
clean:
	rm -f $(EXEC_FILES)
