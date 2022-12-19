BASE = main

all: $(BASE)

OS := $(shell uname -s)

ifeq ($(OS), Linux)
  LIBS += -lGL -lGLU -lGLEW -lglfw
endif

ifeq ($(OS), Darwin)
  CPPFLAGS += -D__MAC__ -std=c++11 -stdlib=libc++
  LDFLAGS += -framework OpenGL -framework IOKit -framework Cocoa
  LIBS += -lglfw.3 -lGLEW
endif

ifdef OPT
  #turn on optimization
  CXXFLAGS += -O2
else
  #turn on debugging
  CXXFLAGS += -g
endif

CXX = g++

SRC_DIR := source
INC_DIR := header
OBJ_DIR := bin-int

SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

# ImGui
IMGUI_DIR := $(INC_DIR)/ImGui

$(BASE): $(OBJ_FILES)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS) -lGLEW

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@ -I$(INC_DIR) -I$(IMGUI_DIR)

$(OBJ_DIR):
	mkdir $@

clean:
	rm -f $(BASE)
	rm -rf $(OBJ_DIR)
