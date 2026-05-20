file(REMOVE_RECURSE
  "CMakeFiles/CompileShaders"
  "shaders/point_light.frag.spv"
  "shaders/point_light.vert.spv"
  "shaders/shadow.frag.spv"
  "shaders/shadow.vert.spv"
  "shaders/simple_shader.frag.spv"
  "shaders/simple_shader.vert.spv"
  "shaders/skybox.frag.spv"
  "shaders/skybox.vert.spv"
  "shaders/vegetation.frag.spv"
  "shaders/vegetation.vert.spv"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/CompileShaders.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
