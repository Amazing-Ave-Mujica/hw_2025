# 设置编译器
CXX = clang++
CXXFLAGS = -std=c++17 -Wall

BUILD_DIR = build
SRC_DIR = src

SRC = $(SRC_DIR)/main.cpp \

OBJ = $(SRC:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)


EXEC = $(BUILD_DIR)/code_craft

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


run:
	python3 tools/run.py tools/interactor.exe data/sample.in build/code_craft.exe

submit:
	@zip -r submission.zip src CMakeLists.txt

format:
	clang-format -i $(SRC_DIR)/*.cpp

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean format run submit
