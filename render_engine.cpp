#include <iostream>
#include <GLES3/gl3.h>
#include <GLES3/gl3platform.h>
#include <stdexcept>
#include <emscripten/val.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5_webgl.h>
#include "shaders.h"
#include <emscripten/bind.h>

using emscripten::val;

#include <stdio.h>

//thread_local const val document = val::global("document");

struct VertexAttributeRef {
  GLuint buffer;
  GLuint loc;
  GLuint byte_length;
};

VertexAttributeRef create_vertex_attribute_ref(
  std::string &vertex_attribute_name,
  int &vertex_attribute_byte_length,
  GLuint program
) {
  VertexAttributeRef vertex_attribute_ref;
  glGenBuffers(1, &vertex_attribute_ref.buffer);
  vertex_attribute_ref.loc = glGetAttribLocation(program, vertex_attribute_name.c_str());
  vertex_attribute_ref.byte_length = vertex_attribute_byte_length;
  return vertex_attribute_ref;
}

int bind_static_vertex_attribute(
  VertexAttributeRef &vertex_attribute_ref, 
  int &vertex_attribute_size,
  int* vertex_data_pointer,
  GLenum data_type = GL_INT
) {

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_attribute_ref.buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertex_attribute_size, vertex_data_pointer, GL_STATIC_DRAW);
  glEnableVertexAttribArray(vertex_attribute_ref.loc);
  glVertexAttribPointer(vertex_attribute_ref.loc,vertex_attribute_ref.byte_length,data_type,false,0,0);

  return 0;
};

int bind_dynamic_vertex_attribute(
  VertexAttributeRef &vertex_attribute_ref, 
  int &stride,
  int &byte_offset,
  int attribute_instances_count = 0,
  GLenum data_type = GL_FLOAT
) {

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_attribute_ref.buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, stride * attribute_instances_count, 0, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(vertex_attribute_ref.loc);
  glVertexAttribPointer(vertex_attribute_ref.loc,vertex_attribute_ref.byte_length,data_type,false,stride,(void*)byte_offset);
  glVertexAttribDivisor(vertex_attribute_ref.loc,1);

  return 0;
};


void compile_shader(
  GLenum shader_type,
  std::string shader_source
) {
  const char *cstr_shader_source = shader_source.c_str();
  const GLuint shader = glCreateShader(shader_type);
  try 
  {
    if (shader) {
      GLint params, compile_status;
      glShaderSource(shader, 1, &cstr_shader_source, NULL);
      glCompileShader(shader);
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &params);
      if (params > 1) {
            GLchar* log_string = new char[params + 1];
            glGetShaderInfoLog(shader, params, 0, log_string);
            throw std::runtime_error (log_string);
            delete[] log_string;
      }
      glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
      if (compile_status != GL_TRUE)
      { 
          throw std::runtime_error ("Exception: Shader not compiled.");
      }
    } else {
      throw std::runtime_error ("Exception: Shader not created.");
    }
  }
  catch(const std::exception& e)
  {
      std::cerr << e.what() << '\n';
  }
};


GLint compile_program(
  std::string vertex_source,
  std::string fragment_source
) {
  GLint program = glCreateProgram();
  GLint params, link_status;
  compile_shader(GL_VERTEX_SHADER, vertex_source);
  compile_shader(GL_FRAGMENT_SHADER, fragment_source);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    GLchar* log_string = new char[params + 1];
    glGetProgramInfoLog(program, params, 0, log_string);
    throw std::runtime_error (log_string);
  }
  return program;
};

class RenderEngine
{
public:
  emscripten::val canvas;
  emscripten::val ctx;

  GLuint vertex_array;
  GLint program;

  std::string vertex_source = shaders::vertex_source_raw;
  std::string fragment_source = shaders::fragment_source_raw;
  std::vector<VertexAttributeRef> vertex_attribute_ref_vec;

  int stride = 0;
  bool initialized = false;
  int attribute_instances_count = 0;

  float canvas_width = 1000;
  float canvas_height = 1000;

  EmscriptenWebGLContextAttributes gl_context_attributes;
  
  float background_color[4] = {0.8118, 0.7843, 0.7843, 0.0};

  RenderEngine(
    std::string canvas_id,

    std::string static_attribute_name = "a_position",
    std::vector<int> static_attribute_data = {1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1},
    int static_attribute_size = 2,
    int static_attribute_byte_length = 4,

    std::vector<std::string> dynamic_attribute_names = {
      "v_box","v_color","v_corner","v_window","v_sigma"
    },
    std::vector<int> dynamic_attribute_byte_lengths = {
      16, 16, 16, 8, 4
    }
  ) {

    //accum stride
    for (auto& n : dynamic_attribute_byte_lengths)
     stride += n;

    gl_context_attributes.alpha = false;
    gl_context_attributes.depth = false;
    gl_context_attributes.stencil = false;

    //might need in addition to emscripten wrapper ...not sure yet
    //ctx = canvas.call<val>("getContext", "webgl2");
    //set_ctx(ctx, ctx_defaults);
    
    emscripten_webgl_create_context(canvas_id.c_str(), &gl_context_attributes);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glClearColor(
      background_color[0],
      background_color[1],
      background_color[2],
      background_color[3]
    );

    program = compile_program(vertex_source, fragment_source);

    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);

    //init static
    vertex_attribute_ref_vec.insert(
      vertex_attribute_ref_vec.begin(), 
      create_vertex_attribute_ref(
        static_attribute_name,static_attribute_byte_length,program)
    );

    bind_static_vertex_attribute(
      vertex_attribute_ref_vec[0], 
      static_attribute_size, 
      static_attribute_data.data()
    );

    //init dynamic
    int _index = 1;
    int _byteOffset = 0;
    for (auto & dynamic_attribute_name : dynamic_attribute_names) {
      vertex_attribute_ref_vec.insert(
        vertex_attribute_ref_vec.begin() + _index, 
        create_vertex_attribute_ref(
          dynamic_attribute_name,
          dynamic_attribute_byte_lengths[_index],
          program
        )
      );
      bind_dynamic_vertex_attribute(
        vertex_attribute_ref_vec[_index],
        stride, _byteOffset
      );
      _index++;
      _byteOffset+=dynamic_attribute_byte_lengths[_index];
    };

    glUseProgram(program);
    initialized = true;
  };
  void _rebind_attributes(int _attribute_instances_count) {
    attribute_instances_count = _attribute_instances_count;
    //rebind dynamic
    int _index = 1;
    int _byteOffset = 0;
    for (
      auto vertex_attribute_ref = vertex_attribute_ref_vec.begin() + 1; //index 0 is ref for static quad
      vertex_attribute_ref!=vertex_attribute_ref_vec.end(); 
      ++vertex_attribute_ref
    ) {
      bind_dynamic_vertex_attribute(
        *vertex_attribute_ref,
        stride, _byteOffset, 
        attribute_instances_count
      );
      _byteOffset+=(*vertex_attribute_ref).byte_length;
    }
  };
  void resize_canvas(float _width, float _height) {
    canvas_width = _width;
    canvas_height = _height;
  };
  void set_background_color(
    float r, float g, float b, float a
  ) {
    background_color[0] = r;
    background_color[1] = g;
    background_color[2] = b;
    background_color[3] = a;
    glClearColor(
      background_color[0],
      background_color[1],
      background_color[2],
      background_color[3]
    );
  };
  void render(
    std::vector<float> _vertex_attribute_data,
    int _attribute_instances_count
  ) {
    if (_attribute_instances_count != attribute_instances_count) {
      _rebind_attributes(_attribute_instances_count);
    };

    glViewport(0,0,canvas_width,canvas_height);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(vertex_array);

    for (
      auto vertex_attribute_ref = vertex_attribute_ref_vec.begin() + 1; //index 0 is ref for static quad
      vertex_attribute_ref!=vertex_attribute_ref_vec.end(); 
      ++vertex_attribute_ref
    ) {
      glBindBuffer(GL_ARRAY_BUFFER, (*vertex_attribute_ref).buffer);
      glBufferSubData(GL_ARRAY_BUFFER, 0,  stride * attribute_instances_count, &_vertex_attribute_data);
    }

  };

};
EMSCRIPTEN_BINDINGS(RenderEngine) {
  emscripten::class_<RenderEngine>("RenderEngine")
    .constructor< 
        std::string,
        std::string,
        std::vector<int>,
        int,
        int,
        std::vector<std::string>,
        std::vector<int>
    >()
    
    .function("resize_canvas", &RenderEngine::resize_canvas)
    .function("set_background_color", &RenderEngine::set_background_color)
    .function("render", &RenderEngine::render)
    ;
}