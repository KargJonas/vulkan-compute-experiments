FROM ubuntu:latest

# Helps avoid some prompts
ENV DEBIAN_FRONTEND=noninteractive

# Update and install necessary packages
RUN apt-get update && apt-get install -y \
    # Build utilities
    pkg-config \
    software-properties-common \
    build-essential \
    # Debugging utilities
    lshw \
    mesa-utils \
    # Vulkan SDK dependencies
    mesa-vulkan-drivers \
    libvulkan-dev \
    vulkan-tools \
    # OpenGL specific stuff
    libglfw3 \
    libglfw3-dev \
    libglm-dev \
    glslang-tools \
    libglew-dev \
    # Toolchain
    cmake \
    # Cleanup
    && apt-get clean

# Set the working directory
WORKDIR /workspace

# Optionally clone a sample Vulkan project (e.g., Vulkan tutorial or your project)
# RUN git clone https://github.com/your_repository_here.git

# Expose any ports you need for your applications (if any)
# EXPOSE 1234

# Set default command
CMD ["/bin/bash"]
