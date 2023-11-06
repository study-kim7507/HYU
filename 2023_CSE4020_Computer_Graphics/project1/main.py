from OpenGL.GL import *
from glfw.GLFW import *
import glm
import ctypes
import numpy as np

g_width = 1000
g_height = 1000

prev_x = None
prev_y = None

target_cam_trans_x = 0.
target_cam_trans_y = 0.

g_cam_ang = 0.
g_cam_height = .01
g_cam_zoom = 50.


isPerspective = True

g_vertex_shader_src = '''
#version 330 core

layout (location = 0) in vec3 vin_pos; 
layout (location = 1) in vec3 vin_color; 

out vec4 vout_color;

uniform mat4 M;

void main()
{
    // 3D points in homogeneous coordinates
    vec4 p3D_in_hcoord = vec4(vin_pos.xyz, 1.0);

    gl_Position = M * p3D_in_hcoord;

    vout_color = vec4(vin_color, 1.);
}
'''

g_fragment_shader_src = '''
#version 330 core

in vec4 vout_color;

out vec4 FragColor;

void main()
{
    FragColor = vout_color;
}
'''

def load_shaders(vertex_shader_source, fragment_shader_source):
    # build and compile our shader program
    # ------------------------------------
    
    # vertex shader 
    vertex_shader = glCreateShader(GL_VERTEX_SHADER)    # create an empty shader object
    glShaderSource(vertex_shader, vertex_shader_source) # provide shader source code
    glCompileShader(vertex_shader)                      # compile the shader object
    
    # check for shader compile errors
    success = glGetShaderiv(vertex_shader, GL_COMPILE_STATUS)
    if (not success):
        infoLog = glGetShaderInfoLog(vertex_shader)
        print("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" + infoLog.decode())
        
    # fragment shader
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER)    # create an empty shader object
    glShaderSource(fragment_shader, fragment_shader_source) # provide shader source code
    glCompileShader(fragment_shader)                        # compile the shader object
    
    # check for shader compile errors
    success = glGetShaderiv(fragment_shader, GL_COMPILE_STATUS)
    if (not success):
        infoLog = glGetShaderInfoLog(fragment_shader)
        print("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" + infoLog.decode())

    # link shaders
    shader_program = glCreateProgram()               # create an empty program object
    glAttachShader(shader_program, vertex_shader)    # attach the shader objects to the program object
    glAttachShader(shader_program, fragment_shader)
    glLinkProgram(shader_program)                    # link the program object

    # check for linking errors
    success = glGetProgramiv(shader_program, GL_LINK_STATUS)
    if (not success):
        infoLog = glGetProgramInfoLog(shader_program)
        print("ERROR::SHADER::PROGRAM::LINKING_FAILED\n" + infoLog.decode())
        
    glDeleteShader(vertex_shader)
    glDeleteShader(fragment_shader)

    return shader_program    # return the shader program


# Mouse 
def click_drag_callback(window, xpos, ypos):
    global prev_x, prev_y, g_cam_ang, g_cam_height, target_cam_trans_x, target_cam_trans_y
    
    # Initalize Position
    if prev_x is None:
        prev_x = xpos
        prev_y = ypos

    delta_x = xpos - prev_x
    delta_y = ypos - prev_y
    

    # Orbit
    if glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS:
        if delta_x < 0:
            g_cam_ang += np.radians(-1)
        elif delta_x > 0:
            g_cam_ang += np.radians(1)

        if delta_y < 0:
            g_cam_height += np.radians(-1)
        elif delta_y > 0:
            g_cam_height += np.radians(1)
        

    # Pan
    elif glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS:
        target_cam_trans_x += delta_x * 0.003
        target_cam_trans_y += -delta_y * 0.003
        

    prev_x = xpos
    prev_y = ypos    

# Mouse Wheel Scroll
def scroll_callback(window, xoffset, yoffset):
    global g_cam_height, g_cam_zoom
    g_cam_zoom += yoffset
    
# KeyBoard
def key_callback(window, key, scancode, action, mods):
    global g_cam_ang, g_cam_height, isPerspective
    if key==GLFW_KEY_ESCAPE and action==GLFW_PRESS:
        glfwSetWindowShouldClose(window, GLFW_TRUE)
    if key==GLFW_KEY_V and action==GLFW_PRESS:
        isPerspective = not isPerspective

def DrawPlaneXZ():
    global vertices
    vertices = []
    for i in range(-10, 11): 
        if i == 0:
            vertices.append(i * 0.1)
            vertices.append(0)
            vertices.append(1)

            vertices.append(0)
            vertices.append(1)
            vertices.append(0)

            # Vertex position 2
            vertices.append(i * 0.1)
            vertices.append(0)
            vertices.append(-1)

            vertices.append(0)
            vertices.append(1)
            vertices.append(0)
        
            # Vertex position 3
            vertices.append(1)
            vertices.append(0)
            vertices.append(i * 0.1)

            vertices.append(1)
            vertices.append(0)
            vertices.append(0)

            # Vertex position 3
            vertices.append(-1)
            vertices.append(0)
            vertices.append(i * 0.1)

            vertices.append(1)
            vertices.append(0)
            vertices.append(0)

            continue

        # Vertex position 1
        vertices.append(i * 0.1)
        vertices.append(0)
        vertices.append(1)

        vertices.append(0.61)
        vertices.append(0.62)
        vertices.append(0.62)

        # Vertex position 2
        vertices.append(i * 0.1)
        vertices.append(0)
        vertices.append(-1)

        vertices.append(0.61)
        vertices.append(0.62)
        vertices.append(0.62)
        
        # Vertex position 3
        vertices.append(1)
        vertices.append(0)
        vertices.append(i * 0.1)

        vertices.append(0.61)
        vertices.append(0.62)
        vertices.append(0.62)

        # Vertex position 3
        vertices.append(-1)
        vertices.append(0)
        vertices.append(i * 0.1)

        vertices.append(0.61)
        vertices.append(0.62)
        vertices.append(0.62)
    
    VAO = glGenVertexArrays(1)
    glBindVertexArray(VAO)

    VBO = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, VBO)

    glBufferData(GL_ARRAY_BUFFER, np.array(vertices, dtype=np.float32), GL_STATIC_DRAW)
    
    # configure vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * glm.sizeof(glm.float32), None)
    glEnableVertexAttribArray(0)

    # configure vertex colors
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * glm.sizeof(glm.float32), ctypes.c_void_p(3*glm.sizeof(glm.float32)))
    glEnableVertexAttribArray(1)

    return VAO

# use orthogonal projection
def renderOrtho():
    CameraPosition = glm.vec3(.1, .3, .1)
    Axis = glm.normalize(CameraPosition)
    Axis = glm.cross(Axis, glm.vec3(0, 1, 0))

    Pan = glm.translate(glm.mat4(), glm.vec3(target_cam_trans_x, target_cam_trans_y, 0))
    Zoom = glm.translate(glm.mat4(), glm.vec3(0, 0, g_cam_zoom * 0.1))
    Azimuth = glm.rotate(g_cam_ang, glm.vec3(0, 1, 0))
    Elevation = glm.rotate(g_cam_height, Axis)

    CameraPosition = glm.vec3(Azimuth * Elevation * glm.vec4(CameraPosition, 1.0))
    

    P = glm.ortho(-5, 5, -5, 5, -10, 10)
    V = glm.lookAt(CameraPosition, (0, 0, 0), (0, 1, 0))
    
    return P * Zoom * Pan * V 

# use perspective projection
def renderPerspective():
    global CameraPosition
    CameraPosition = glm.vec3(3, 5, 3)

    Axis = glm.normalize(CameraPosition)
    Axis = glm.cross(Axis, glm.vec3(0, 1, 0))

    Pan = glm.translate(glm.mat4(), glm.vec3(target_cam_trans_x, target_cam_trans_y, 0))
    Zoom = glm.translate(glm.mat4(), glm.vec3(0 , 0 , g_cam_zoom * 0.1))

    Azimuth = glm.rotate(g_cam_ang, glm.vec3(0, 1, 0))
    Elevation = glm.rotate(g_cam_height, Axis)
    
    CameraPosition = glm.vec3(Azimuth * Elevation * glm.vec4(CameraPosition, 1.0))


    P = glm.perspective(45, 1, 1, 10)
    V = glm.lookAt(CameraPosition, (0, 0, 0), (0, 1, 0))

        
    return P * Zoom * Pan * V

def main():
    # initialize glfw
    if not glfwInit():
        return
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3)   # OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE)  # Do not allow legacy OpenGl API calls
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE) # for macOS

    # create a window and OpenGL context
    window = glfwCreateWindow(1000, 1000, 'Project #1. 2017029343. 김기환', None, None)
    if not window:
        glfwTerminate()
        return
    glfwMakeContextCurrent(window)

    # register event callbacks
    glfwSetCursorPosCallback(window, click_drag_callback)
    glfwSetScrollCallback(window, scroll_callback)
    glfwSetKeyCallback(window, key_callback)
    
    # load shaders
    shader_program = load_shaders(g_vertex_shader_src, g_fragment_shader_src)

    # get uniform locations
    M_loc = glGetUniformLocation(shader_program, 'M')
    
    vao_plane = DrawPlaneXZ()

    # loop until the user closes the window
    while not glfwWindowShouldClose(window):
        # render

        # enable depth test (we'll see details later)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glClearColor(.25, .25, .25, 0.0)
        glEnable(GL_DEPTH_TEST)

        glUseProgram(shader_program)

        if (isPerspective == False):
            retMatrix = renderOrtho() 
        elif (isPerspective == True):
            retMatrix = renderPerspective()   

        # current frame: P*V*I (now this is the world frame)
        I = glm.mat4()
        MVP = retMatrix * I
        glUniformMatrix4fv(M_loc, 1, GL_FALSE, glm.value_ptr(MVP))

        glBindVertexArray(vao_plane)
        glDrawArrays(GL_LINES, 0, len(vertices))

        # swap front and back buffers
        glfwSwapBuffers(window)

        # poll events
        glfwPollEvents()

    # terminate glfw
    glfwTerminate()

if __name__ == "__main__":
    main()
