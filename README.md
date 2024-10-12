
### Build docker container with dev environment
```bash
docker build -t vulkan-dev .
```

### Enter docker container
```bash
docker run --device /dev/dri -it -v .:/workspace vulkan-dev
```
Note: Without the `--device /dev/dri` option, the GPU will not be accessible from inside the container, thus VK will fall back on LLVM.

### A note on environment variables:
If you are using 

### Resources
- https://vulkan-tutorial.com/en/Overview
- https://vulkan-tutorial.com/Compute_Shader
- https://github.com/Overv/VulkanTutorial/tree/main/code