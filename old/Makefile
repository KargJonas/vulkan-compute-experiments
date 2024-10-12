# Makefile for Vulkan Tutorial - Single Chapter (Last One)

# Paths (Modify these paths according to your system setup)
VULKAN_SDK_PATH := /path/to/vulkan-sdk
GLFW_PATH := /usr/include
GLM_PATH := /usr/include

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++17 -I$(GLFW_PATH) -I$(GLM_PATH) -I$(VULKAN_SDK_PATH)/include
LDFLAGS := -L$(VULKAN_SDK_PATH)/lib
LDLIBS := -lvulkan -lglfw -ldl -lpthread

# Source and Object files
CHAPTER_NAME := compute_shader
SOURCE_FILE := $(CHAPTER_NAME).cpp
OBJECT_FILE := $(SOURCE_FILE:.cpp=.o)

# Shader files
SHADERS_SRC := $(wildcard shaders/*.glsl)
SHADERS_SPV := $(SHADERS_SRC:.glsl=.spv)

# Executable build rule
.PHONY: all
all: $(CHAPTER_NAME)

$(CHAPTER_NAME): $(OBJECT_FILE) $(SHADERS_SPV)
	$(CXX) $(OBJECT_FILE) $(LDFLAGS) $(LDLIBS) -o $@

# Object file compilation
$(OBJECT_FILE): $(SOURCE_FILE)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Shader compilation rule
shaders/%.spv: shaders/%.glsl
	$(VULKAN_SDK_PATH)/bin/glslangValidator --target-env vulkan1.0 -V $< -o $@

# Clean up
.PHONY: clean
clean:
	rm -f $(OBJECT_FILE) $(CHAPTER_NAME) $(SHADERS_SPV)

# Include dependencies
-include $(OBJECTS:.o=.d)
