#ifndef RENDER_ENGINE_H
#define RENDER_ENGINE_H

#include <GLES3/gl3.h>
#include <GLES3/gl3platform.h>
#include <emscripten/val.h>
#include <emscripten/html5_webgl.h>

using namespace emscripten;

struct VertexAttributeRef {
  GLuint buffer;
  GLuint loc;
  GLuint byte_length;
};

class RenderEngine {
public:
  emscripten::val canvas;
  emscripten::val ctx;

  GLuint vertex_array;
  GLint program;

  std::string vertex_source;
  std::string fragment_source;
  std::vector<VertexAttributeRef> vertex_attribute_ref_vec;

  int stride;
  bool initialized;
  int attribute_instances_count;

  float canvas_width;
  float canvas_height;

  EmscriptenWebGLContextAttributes gl_context_attributes;

  float background_color[4];

  RenderEngine(
    std::string canvas_id,
    std::string static_attribute_name,
    std::vector<int> static_attribute_data,
    int static_attribute_size,
    int static_attribute_byte_length,
    std::vector<std::string> dynamic_attribute_names,
    std::vector<int> dynamic_attribute_byte_lengths
  );

  void _rebind_attributes(int _attribute_instances_count);
  void resize_canvas(float _width, float _height);
  void set_background_color(float r, float g, float b, float a);
  void render(std::vector<float> _vertex_attribute_data, int _attribute_instances_count);
};


#endif // RENDER_ENGINE_H
