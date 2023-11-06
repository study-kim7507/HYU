from OpenGL.GL import *
from glfw.GLFW import *
import glm
import ctypes
import numpy as np
import os

g_width = 1000
g_height = 1000

prev_x = None
prev_y = None

target_cam_trans_x = 0.
target_cam_trans_y = 0.

CameraPosition = glm.vec3(0, 0, 0)

g_cam_ang = 0.
g_cam_height = .01
g_cam_zoom = 50.

isPerspective = True

isDropped = False
isAnimation = False
isBox = False
nodes = []
count = 0

g_vertex_shader_for_line = '''
#version 330 core

layout (location = 0) in vec3 vin_pos; 

out vec4 vout_color;

uniform mat4 MVP;
uniform vec3 color;

void main()
{
    // 3D points in homogeneous coordinates
    vec4 p3D_in_hcoord = vec4(vin_pos.xyz, 1.0);

    gl_Position = MVP * p3D_in_hcoord;

    vout_color = vec4(color, 1.);
    //vout_color = vec4(1,1,1,1);
}
'''

g_vertex_shader_for_box = '''
#version 330 core

layout (location = 0) in vec3 vin_pos; 
layout (location = 1) in vec3 vin_normal; 

out vec3 vout_surface_pos;
out vec3 vout_normal;

uniform mat4 MVP;
uniform mat4 M;

void main()
{
    vec4 p3D_in_hcoord = vec4(vin_pos.xyz, 1.0);
    gl_Position = MVP * p3D_in_hcoord;

    vout_surface_pos = vec3(M * vec4(vin_pos, 1));
    vout_normal = normalize( mat3(inverse(transpose(M)) ) * vin_normal);
}
'''

g_fragment_shader_for_line = '''
#version 330 core

in vec4 vout_color;

out vec4 FragColor;

void main()
{
    FragColor = vout_color;
}
'''

g_fragment_shader_for_box = '''
#version 330 core

in vec3 vout_surface_pos;
in vec3 vout_normal;  // interpolated normal

out vec4 FragColor;

uniform vec3 view_pos;

void main()
{
    // light and material properties
    vec3 light_pos = vec3(3,2,4);
    vec3 light_color = vec3(1,1,1);
    vec3 material_color = vec3(1,1,0);
    float material_shininess = 30.0;

    // light components
    vec3 light_ambient = 0.1*light_color;
    vec3 light_diffuse = light_color;
    vec3 light_specular = light_color;

    // material components
    vec3 material_ambient = material_color;
    vec3 material_diffuse = material_color;
    vec3 material_specular = vec3(1,1,1);  // for non-metal material

    // ambient
    vec3 ambient = light_ambient * material_ambient;

    // for diffiuse and specular
    vec3 normal = normalize(vout_normal);
    vec3 surface_pos = vout_surface_pos;
    vec3 light_dir = normalize(light_pos - surface_pos);

    // diffuse
    float diff = max(dot(normal, light_dir), 0);
    vec3 diffuse = diff * light_diffuse * material_diffuse;

    // specular
    vec3 view_dir = normalize(view_pos - surface_pos);
    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow( max(dot(view_dir, reflect_dir), 0.0), material_shininess);
    vec3 specular = spec * light_specular * material_specular;

    vec3 color = ambient + diffuse + specular;
    FragColor = vec4(color, 1.);
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

# Mouse - Click and Drag 
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

# Mouse - Wheel Scroll
def scroll_callback(window, xoffset, yoffset):
    global g_cam_height, g_cam_zoom
    g_cam_zoom += yoffset
    
# KeyBoard
def key_callback(window, key, scancode, action, mods):
    global g_cam_ang, g_cam_height, isPerspective, isBox, isAnimation
    if key==GLFW_KEY_ESCAPE and action==GLFW_PRESS:
        glfwSetWindowShouldClose(window, GLFW_TRUE)
    if key==GLFW_KEY_V and action==GLFW_PRESS:
        isPerspective = not isPerspective
    if key==GLFW_KEY_1 and action==GLFW_PRESS:
        isBox = False
    if key==GLFW_KEY_2 and action==GLFW_PRESS:
        isBox = True
    if key==GLFW_KEY_SPACE and action==GLFW_PRESS:
        isAnimation = True

class Node:
    def __init__(self, name):
        self.name = name
        self.offset = None
        self.difference = None
        self.channel_num = None
        self.channels = None
        self.parent = None
        self.children = []
        self.motionData = []
    
        #transform
        self.link_transform_from_parent = glm.mat4()
        self.joint_transform = glm.mat4()
        self.global_transform = glm.mat4()
    
        # shape
        self.shape_transform = glm.mat4()

    def set_link_transform_from_parent(self, link_transform_from_parent):
        self.link_transform_from_parent = link_transform_from_parent

    def set_joint_transform(self, joint_transform):
        self.joint_transform = joint_transform
    
    def set_shape_transform(self, shape_transform):
        self.shape_transform = shape_transform

    def get_link_transform_from_parent(self):
        return self.link_transform_from_parent
    
    def get_joint_transform(self):
        return self.joint_transform
    
    def get_global_transform(self):
        return self.global_transform
    
    def update_tree_global_transform(self):
        if self.parent is not None:
            self.global_transform = self.parent.get_global_transform() * self.link_transform_from_parent * self.joint_transform
        else:
            self.global_transform = self.link_transform_from_parent * self.joint_transform
        
        for child in self.children:
            child.update_tree_global_transform()

def drop_callback(window, paths):
    global isDropped, isAnimation, isBox
    isDropped = True
    isAnimation = False
    isBox = False

    parsingBVH(paths)
    infoBVH("".join(paths))

def parsingBVH(paths):
    global nodes, frames, frame_time, joint_num, count
    file = open("".join(paths))

    joint_num = 0
    nodes = []
    root_node = None
    parent_node = None
    current_node = None
    isMotion = False
    frames = 0
    frame_time = 0
    motionData = []
    count = 0

    while True:
        line = file.readline()

        # End of file
        if not line:
            break
            
        line = ' '.join(line.split())
        line = line.split(' ')

        if 'ROOT' in line or 'JOINT' in line or 'End' in line:
            joint_name = line[1]
            if current_node:
                parent_node = current_node
            current_node = Node(joint_name)

            if root_node is None:
                root_node = current_node

            current_node.parent = parent_node

            if current_node.parent is not None:
                current_node.parent.children.append(current_node)

            if 'ROOT' in line or 'JOINT' in line:
                joint_num += 1

            nodes.append(current_node)


        elif 'OFFSET' in line:
            current_node.offset = [float(line[1]), float(line[2]), float(line[3])]
            current_node.offset = np.array(current_node.offset, dtype=np.float64)
            
        elif 'CHANNELS' in line:
            channels = []
            current_node.channel_num = int(line[1])
            for i in range(0, int(line[1])):
                channels.append(line[i + 2].upper())
            current_node.channels = channels

        elif '{' in line:
            pass

        elif '}' in line:
            
            current_node = current_node.parent
            if current_node is not None:
                parent_node = current_node.parent

        elif 'MOTION' in line:
            isMotion = True
        
        elif 'Frames:' in line:
            frames = int(line[1])

        elif 'Frame' in line:
            frame_time = float(line[2])

        elif isMotion:
            motionData = np.array([float(data) for data in line], dtype=np.float64)
            posData = np.radians(motionData[:3])
            if (max(posData)) < 0.1:
                posData *= 100
            motionData[:3] = posData

            for node in nodes:
                if node.name == "Site":
                    continue
                
                jointData = motionData[:node.channel_num]
                motionData = motionData[node.channel_num:]
                node.motionData.append(jointData)
        
    maxOffset = 0.0
    for node in nodes:
        if maxOffset < max(node.offset):
            maxOffset = max(node.offset)
    
    for node in nodes:
        node.offset = node.offset / maxOffset
        node.set_link_transform_from_parent(glm.translate(glm.vec3(node.offset)))

    nodes[0].update_tree_global_transform()

def infoBVH(paths):
    global frames, frame_time, joint_num, nodes
    fileName = paths.split('\\')[-1]
    print("File Name : " + fileName)
    print("Number of frames : " + str(frames))
    print("FPS : " + str(1/frame_time))
    print("Number of joints : " + str(joint_num))
    print("List of all joint :", end = ' ')
    for i in nodes:
        if i.name == "Site":
            continue
        print(i.name, end = ' ')
    print()

def prepare_vao_frameX():
    vertices = glm.array(glm.float32,
        # position     
        -5,   0,   0,
         5,   0,   0,
        )
    
    indices = glm.array(glm.uint32,
        0, 1,
    )
    
    VAO = glGenVertexArrays(1)
    glBindVertexArray(VAO)

    VBO = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, VBO)

    EBO = glGenBuffers(1)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)

    glBufferData(GL_ARRAY_BUFFER, np.array(vertices, dtype=np.float32), GL_STATIC_DRAW)
    
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices.ptr, GL_STATIC_DRAW)

    # configure vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * glm.sizeof(glm.float32), None)
    glEnableVertexAttribArray(0)

    return VAO

def prepare_vao_frameZ():
    global vertices
    vertices = glm.array(glm.float32,
        # position         
         0,   0,  -5,   
         0,   0,   5,
        )
    
    indices = glm.array(glm.uint32,
        0, 1,
    )
    
    VAO = glGenVertexArrays(1)
    glBindVertexArray(VAO)

    VBO = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, VBO)

    EBO = glGenBuffers(1)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)

    glBufferData(GL_ARRAY_BUFFER, np.array(vertices, dtype=np.float32), GL_STATIC_DRAW)
    
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices.ptr, GL_STATIC_DRAW)

    # configure vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * glm.sizeof(glm.float32), None)
    glEnableVertexAttribArray(0)

    return VAO

def DrawPlane(vao_frameX, vao_frameZ, MVP, MVP_loc, color_loc):
    for i in range(-50, 51):
                glBindVertexArray(vao_frameX)
                MVP_X = MVP * glm.translate(glm.vec3(0, 0, i * 0.1))
                glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, glm.value_ptr(MVP_X))
                if i != 0:
                    glUniform3f(color_loc, 1, 1, 1)
                elif i == 0:
                    glUniform3f(color_loc, 1, 0, 0)
                glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, None)

                glBindVertexArray(vao_frameZ)
                MVP_Z = MVP * glm.translate(glm.vec3(i * 0.1, 0, 0))
                if i != 0:
                    glUniform3f(color_loc, 1, 1, 1)
                elif i == 0:
                    glUniform3f(color_loc, 0, 0, 1)
                glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, glm.value_ptr(MVP_Z))
                glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, None)

# use orthogonal projection
def renderOrtho():
    global CameraPosition
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
    
    P = glm.perspective(45, 1, 1, 10000)
    V = glm.lookAt(CameraPosition, (0, 0, 0), (0, 1, 0))

    return P * Zoom * Pan * V

def prepare_vao_cube():
        # prepare vertex data (in main memory)
    # 8 vertices
    vertices = glm.array(glm.float32,
        # position      normal
        -1 ,  1 ,  1 , -0.577 ,  0.577,  0.577, # v0
         1 ,  1 ,  1 ,  0.816 ,  0.408,  0.408, # v1
         1 , -1 ,  1 ,  0.408 , -0.408,  0.816, # v2
        -1 , -1 ,  1 , -0.408 , -0.816,  0.408, # v3
        -1 ,  1 , -1 , -0.408 ,  0.408, -0.816, # v4
         1 ,  1 , -1 ,  0.408 ,  0.816, -0.408, # v5
         1 , -1 , -1 ,  0.577 , -0.577, -0.577, # v6
        -1 , -1 , -1 , -0.816 , -0.408, -0.408, # v7
    )

    # prepare index data
    # 12 triangles
    indices = glm.array(glm.uint32,
        0,2,1,
        0,3,2,
        4,5,6,
        4,6,7,
        0,1,5,
        0,5,4,
        3,6,2,
        3,7,6,
        1,2,6,
        1,6,5,
        0,7,3,
        0,4,7,
    )
    # create and activate VAO (vertex array object)
    VAO = glGenVertexArrays(1)  # create a vertex array object ID and store it to VAO variable
    glBindVertexArray(VAO)      # activate VAO

    # create and activate VBO (vertex buffer object)
    VBO = glGenBuffers(1)   # create a buffer object ID and store it to VBO variable
    glBindBuffer(GL_ARRAY_BUFFER, VBO)  # activate VBO as a vertex buffer object

    # create and activate EBO (element buffer object)
    EBO = glGenBuffers(1)   # create a buffer object ID and store it to EBO variable
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)  # activate EBO as an element buffer object

    # copy vertex data to VBO
    glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices.ptr, GL_STATIC_DRAW) # allocate GPU memory for and copy vertex data to the currently bound vertex buffer

    # copy index data to EBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices.ptr, GL_STATIC_DRAW) # allocate GPU memory for and copy index data to the currently bound element buffer

    # configure vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * glm.sizeof(glm.float32), None)
    glEnableVertexAttribArray(0)

    # configure vertex normals
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * glm.sizeof(glm.float32), ctypes.c_void_p(3*glm.sizeof(glm.float32)))
    glEnableVertexAttribArray(1)

    return VAO

def prepare_vao_line(nodes):
    vertices = []
    for node in nodes:
        if node.parent == None:
            continue
        
        PglobalMat = node.parent.global_transform[3]
        CglobalMat = node.global_transform[3]
        vertices.append([PglobalMat[0], PglobalMat[1], PglobalMat[2]])
        vertices.append([CglobalMat[0], CglobalMat[1], CglobalMat[2]])
    vertices = np.array(vertices, dtype=np.float64)
    vertices = glm.array(vertices.flatten())

    VAO = glGenVertexArrays(1)
    glBindVertexArray(VAO)

    VBO = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, VBO)

    glBufferData(GL_ARRAY_BUFFER, np.array(vertices, dtype=np.float32), GL_STATIC_DRAW)
    
    # configure vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * glm.sizeof(glm.float32), None)
    glEnableVertexAttribArray(0)

    return VAO, len(vertices)/3

def draw_line(nodes, MVP, MVP_loc, color_loc):
    vao_line, length = prepare_vao_line(nodes)

    glBindVertexArray(vao_line)
    glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, glm.value_ptr(MVP))
    glUniform3f(color_loc, 0, 0, 0.8)
    glDrawArrays(GL_LINES, 0, int(length))

def draw_node(vao, node, VP, M, view_pos, MVP_loc, M_loc, view_pos_loc):
    node.shape_transform = glm.scale((0.15, 0.15, 0.15))
    MVP = VP * node.global_transform * node.shape_transform

    glBindVertexArray(vao)
    glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, glm.value_ptr(MVP))
    glUniformMatrix4fv(M_loc, 1, GL_FALSE, glm.value_ptr(M))
    glUniform3f(view_pos_loc, view_pos.x, view_pos.y, view_pos.z)
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, None)

def make_motion():
    global count, nodes, frames
    for node in nodes:
        if node.name == "Site":
            continue

        if "XPOSITION" in node.channels:
            xPosIdx = node.channels.index("XPOSITION")
        if "YPOSITION" in node.channels:
            yPosIdx = node.channels.index("YPOSITION")
        if "ZPOSITION" in node.channels:
            zPosIdx = node.channels.index("ZPOSITION")
        
        if "XROTATION" in node.channels:
            xRotateIdx = node.channels.index("XROTATION")
        if "YROTATION" in node.channels:
            yRotateIdx = node.channels.index("YROTATION")
        if "ZROTATION" in node.channels:
            zRotateIdx = node.channels.index("ZROTATION")
        
        motion = node.motionData[count]
        
        if node.parent == None:
            firPos = motion[0]
            secPos = motion[1]
            thrPos = motion[2]
            firR = motion[3]
            secR = motion[4]
            thrR = motion[5]
            
        else:
            firR = motion[0]
            secR = motion[1]
            thrR = motion[2]
        
        # Root node
        transMat = glm.mat4()
        rotateMat = glm.mat4()
        if node.parent == None:
            transMat = glm.translate(glm.mat4(), glm.vec3(firPos, secPos, thrPos))
        
        # 1 2 3 x y z
        if xRotateIdx < yRotateIdx and yRotateIdx < zRotateIdx:
            rotateMat = glm.rotate(np.radians(firR), (1, 0, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(secR), (0, 1, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(thrR), (0, 0, 1))
        # 2 1 3 y x z
        elif yRotateIdx < xRotateIdx and xRotateIdx < zRotateIdx:
            rotateMat = glm.rotate(np.radians(firR), (0, 1, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(secR), (1, 0, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(thrR), (0, 0, 1))
        # 1 3 2 x z y
        elif xRotateIdx < zRotateIdx and zRotateIdx < yRotateIdx:
            rotateMat = glm.rotate(np.radians(firR), (1, 0, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(secR), (0, 0, 1))
            rotateMat = rotateMat @ glm.rotate(np.radians(thrR), (0, 1, 0))
        # 3 2 1 z y x
        elif zRotateIdx < yRotateIdx and yRotateIdx < xRotateIdx:
            rotateMat = glm.rotate(np.radians(firR), (0, 0, 1))
            rotateMat = rotateMat @ glm.rotate(np.radians(secR), (0, 1, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(thrR), (1, 0, 0))
        # 3 1 2 y z x
        elif yRotateIdx < zRotateIdx and zRotateIdx < xRotateIdx:
            rotateMat = glm.rotate(np.radians(firR), (0, 1, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(secR), (0, 0, 1))
            rotateMat = rotateMat @ glm.rotate(np.radians(thrR), (1, 0, 0))
        # 2 3 1 z x y
        elif zRotateIdx < xRotateIdx and xRotateIdx < yRotateIdx:
            rotateMat = glm.rotate(np.radians(firR), (0, 0, 1))
            rotateMat = rotateMat @ glm.rotate(np.radians(secR), (1, 0, 0))
            rotateMat = rotateMat @ glm.rotate(np.radians(thrR), (0, 1, 0))

        node.set_joint_transform(transMat @ rotateMat)
    
    count += 1
    if (count == frames):   
        count = 0

def main():
    global nodes, count

    # initialize glfw
    if not glfwInit():
        return
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3)   # OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE)  # Do not allow legacy OpenGl API calls
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE) # for macOS

    # create a window and OpenGL context
    window = glfwCreateWindow(800, 800, '2017029343-Project3', None, None)
    if not window:
        glfwTerminate()
        return
    glfwMakeContextCurrent(window)

    # register event callbacks
    glfwSetCursorPosCallback(window, click_drag_callback)
    glfwSetScrollCallback(window, scroll_callback)
    glfwSetKeyCallback(window, key_callback)
    glfwSetDropCallback(window, drop_callback)

    # load shaders
    shader_for_line = load_shaders(g_vertex_shader_for_line, g_fragment_shader_for_line)
    shader_for_box = load_shaders(g_vertex_shader_for_box, g_fragment_shader_for_box)
    
    # get uniform locations
    MVP_loc_line = glGetUniformLocation(shader_for_line, 'MVP')
    color_loc_line = glGetUniformLocation(shader_for_line, 'color')

    MVP_loc_box = glGetUniformLocation(shader_for_box, 'MVP')
    M_loc_box = glGetUniformLocation(shader_for_box, 'M')
    view_pos_loc_box = glGetUniformLocation(shader_for_box, 'view_pos')

    # prepares vaos    
    vao_frameX = prepare_vao_frameX()
    vao_frameZ = prepare_vao_frameZ()

    vao_cube = prepare_vao_cube()

    # loop until the user closes the window
    while not glfwWindowShouldClose(window):
        # enable depth test (we'll see details later)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        # glClearColor(.25, .25, .25, 0.0)
        glEnable(GL_DEPTH_TEST)

        if (isPerspective == False):
            VP = renderOrtho() 
        elif (isPerspective == True):
            VP = renderPerspective()   

        M = glm.mat4()
        MVP = VP * M
        view_pos = CameraPosition

        glUseProgram(shader_for_line)
        DrawPlane(vao_frameX, vao_frameZ, MVP, MVP_loc_line, color_loc_line)
        
        if isDropped == True and isAnimation == False:
            nodes[0].update_tree_global_transform()
            draw_line(nodes, MVP, MVP_loc_line, color_loc_line)

        elif isDropped == True and isBox == False:
            nodes[0].update_tree_global_transform()
            make_motion()
            draw_line(nodes, MVP, MVP_loc_line, color_loc_line)
            
        elif isDropped == True and isBox == True:
            glUseProgram(shader_for_box)
            nodes[0].update_tree_global_transform()
            make_motion()        
            
            for i in range(0, len(nodes)):
                draw_node(vao_cube, nodes[i], VP, M, view_pos, MVP_loc_box, M_loc_box, view_pos_loc_box)
        
        # swap front and back buffers
        glfwSwapBuffers(window)

        # poll events
        glfwPollEvents()

    # terminate glfw
    glfwTerminate()

if __name__ == "__main__":
    main()


