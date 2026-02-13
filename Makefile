CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
LDFLAGS := -pthread

SRC_DIR := src
SRCS := $(SRC_DIR)/main.cpp $(SRC_DIR)/Logger.cpp $(SRC_DIR)/gpu_reader.cpp $(SRC_DIR)/monitor.cpp
OBJS := $(SRCS:.cpp=.o)
TARGET := procmon

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
