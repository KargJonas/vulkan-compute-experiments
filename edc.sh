#/bin/bash

# -v /usr/share/vulkan:/usr/share/vulkan:ro \
# -v /usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:ro \
# --env MESA_LOADER_DRIVER_OVERRIDE=iris \

docker run \
  --rm \
  -e TERM=xterm-256color \
  --device /dev/dri \
  -v .:/workspace \
  -it vulkan-dev