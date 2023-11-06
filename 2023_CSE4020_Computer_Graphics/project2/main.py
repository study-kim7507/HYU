from OpenGL.GL import *
from glfw.GLFW import *
import glm
import ctypes
import numpy as np
import os

np.seterr(divide='ignore', invalid='ignore')

g_width = 1000
g_height = 1000

prev_x = None
prev_y = None

target_cam_trans_x = 0.
target_cam_trans_y = 0.

g_cam_ang = 0.
g_cam_height = .01
g_cam_zoom = 50.

CameraPosition = glm.vec3(0, 0, 0)

isDropped = False
isSingleMeshReady = False
isWireframeMode = False
isHierarchy = False
isPerspective = True

g_vertex_shader_src_color_attribute = '''
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
}
'''

g_vertex_shader_src_color_uniform = '''
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
    vout_normal = normalize(mat3(inverse(transpose(M))) * vin_normal);
}
'''

g_fragment_shader_src_color_attribute = '''
#version 330 core

in vec4 vout_color;

out vec4 FragColor;

void main()
{
    FragColor = vout_color;
}
'''

g_fragment_shader_src_color_uniform = '''
#version 330 core

in vec3 vout_surface_pos;
in vec3 vout_normal;

out vec4 FragColor;

uniform vec3 view_pos;
uniform vec3 color;
uniform mat4 change_light_pos;

void main()
{
    // light and material properties
    vec3 light_pos = vec3(100,100,100);
    light_pos = vec3(change_light_pos * vec4(light_pos, 1.0));
    vec3 light_color = vec3(1,1,1);
    vec3 material_color = color;
    float material_shininess = 32.0;

    // light components
    vec3 light_ambient = 0.5 * light_color;
    vec3 light_diffuse = light_color;
    vec3 light_specular = light_color;

    // material components
    vec3 material_ambient = material_color;
    vec3 material_diffuse = material_color;
    vec3 material_specular = light_color;  // for non-metal material

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


# Structure for holding object information
class ObjectInfo:
    def __init__(self):
        self.paths = ""
        self.vertex_pos = np.array([[0,0,0]], 'float32')
        self.vertex_norm = np.array([[0,0,0]], 'float32')
        self.index = np.array([[0,0,0]], 'int32')
        self.count = [0, 0, 0]
        self.MAX = np.finfo(np.float32).min

    def set_MAX(self, MAX):
        self.MAX = MAX
    def set_paths(self, paths):
        self.paths = paths
    def set_vertex_pos(self, vertex_pos):
        self.vertex_pos = vertex_pos
    def set_vertex_norm(self, vertex_norm):
        self.vertex_norm = vertex_norm
    def set_index(self, index):
        self.index = index
    def set_count(self, count):
        self.count = count

    def get_MAX(self):
        return self.MAX
    def get_paths(self):
        return self.paths
    def get_vertex_pos(self):
        return self.vertex_pos
    def get_vertex_norm(self):
        return self.vertex_norm
    def get_index(self):
        return self.index
    def get_count(self):
        return self.count

# For Single Mesh Rendering Mode
Single = ObjectInfo()    
    
# For Hierarchical Mesh Rendering Mode
Girl = ObjectInfo()
ShoppingCart1 = ObjectInfo()
ShoppingCart2 = ObjectInfo()
Burger = ObjectInfo()
RoastChicken = ObjectInfo()
Bottle = ObjectInfo()
PlasticBox = ObjectInfo()

# For Hierarchical Model Rendering
class Node:
    def __init__(self, parent, shape_transform, color):
        # hierarchy
        self.parent = parent
        self.children = []
        if parent is not None:
            parent.children.append(self)

        # transform
        self.transform = glm.mat4()
        self.global_transform = glm.mat4()

        # shape
        self.shape_transform = shape_transform
        self.color = color

    def set_transform(self, transform):
        self.transform = transform

    def update_tree_global_transform(self):
        if self.parent is not None:
            self.global_transform = self.parent.get_global_transform() * self.transform
        else:
            self.global_transform = self.transform

        for child in self.children:
            child.update_tree_global_transform()

    def get_global_transform(self):
        return self.global_transform
    def get_shape_transform(self):
        return self.shape_transform
    def get_color(self):
        return self.color

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
    global g_cam_ang, g_cam_height, isPerspective, isWireframeMode, isHierarchy, isSingleMeshReady
    if key==GLFW_KEY_ESCAPE and action==GLFW_PRESS:
        glfwSetWindowShouldClose(window, GLFW_TRUE)
    if key==GLFW_KEY_V and action==GLFW_PRESS:
        isPerspective = not isPerspective
    if key==GLFW_KEY_H and action==GLFW_PRESS:
        isHierarchy = True
        isSingleMeshReady = False
    if key==GLFW_KEY_Z and action==GLFW_PRESS:
        if isWireframeMode:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
        else:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
        isWireframeMode = not(isWireframeMode)
    
# Drop obj
def drop_callback(window, paths):
    global isDropped, isSingleMeshReady, Single, isHierarchy
    isDropped = True
    isSingleMeshReady = True
    isHierarchy = False

    print("Parsing the obj file may take some time. Please wait.")
    Single = ObjectInfo()
    parsingObject(paths, Single)
    print("The parsing is complete. Now rendering the model in 3D space.")

    infoObj("".join(paths), Single)
            
def prepare_vao_frameX():
    global vertices
    vertices = glm.array(glm.float32,
        # position     
        -2,   0,   0,
         2,   0,   0,
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
         0,   0,  -2,   
         0,   0,   2,
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
    for i in range(-20, 21):
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

def infoObj(paths, objectInfo):
    count = objectInfo.get_count()
    filename = paths.split('\\')[-1]
    print("Object file name : " + filename)
    print("Total number of faces : " + str(count[0] + count[1] + count[2]))
    print("Number of faces with 3 vertices : " + str(count[0]))
    print("Number of faces with 4 vertices : " + str(count[1]))
    print("Number of faces with 5 vertices : " + str(count[2]))

def parsingObject(paths, objectInfo): 
    file = open("".join(paths))
    
    vertex_pos = np.array([[0,0,0]], 'float32')
    vertex_norm = np.array([[0,0,0]], 'float32')
    index = np.array([[0,0,0]], 'int32')
    count = [0, 0, 0]

    MAX = objectInfo.get_MAX()

    while True:
        line = file.readline()

        # End of file
        if not line:
            break
            
        line = ' '.join(line.split())
        line = line.split(' ')
        
        lineFirst = line[0]

        if not(lineFirst == 'v' or lineFirst == 'vn' or lineFirst == 'f'):
            continue

        elif lineFirst == 'v':
            pos = np.array([[float(line[1]), float(line[2]), float(line[3])]], 'float32')
            vertex_pos = np.append(vertex_pos, pos, axis=0)
            if abs(pos.min()) < abs(pos.max()): 
                absMax = pos.max() 
            else: 
                absMax = abs(pos.min())
            if MAX < absMax:
                MAX = absMax
            
        elif lineFirst == 'vn':
            vertex_norm = np.append(vertex_norm, np.array([[float(line[1]), float(line[2]), float(line[3])]], 'float32'), axis=0)
        
        elif lineFirst == 'f':
            line[-1] = line[-1].strip('\n')

            if len(line) - 1 == 3:
                count[0] += 1
            elif len(line) - 1 == 4:
                count[1] += 1
            elif len(line) - 1 >= 5:
                count[2] += 1
            
            for i in range(0, len(line) - 3):
                index = np.append(index, np.array([[int((line[1].split("/"))[0]), int((line[2 + i].split("/"))[0]), int((line[3 + i].split("/"))[0])]]), axis=0)                 
            
    norm = np.zeros((len(vertex_pos), 3))
    for i in range(1, len(index)):
        a = vertex_pos[index[i][1]] - vertex_pos[index[i][0]]
        b = vertex_pos[index[i][2]] - vertex_pos[index[i][0]]
        crossP = np.cross(a, b)
        norm[index[i][0]] += crossP / np.linalg.norm(crossP)
        a = vertex_pos[index[i][2]] - vertex_pos[index[i][1]]
        b = vertex_pos[index[i][0]] - vertex_pos[index[i][1]]
        crossP = np.cross(a, b)
        norm[index[i][1]] += crossP / np.linalg.norm(crossP)
        a = vertex_pos[index[i][0]] - vertex_pos[index[i][2]]
        b = vertex_pos[index[i][1]] - vertex_pos[index[i][2]]
        crossP = np.cross(a, b)
        norm[index[i][2]] += crossP / np.linalg.norm(crossP)


    # calculate vertex normals
    for i in range(1, len(norm)):
        norm[i] = norm[i] / np.linalg.norm(norm[i])

    vertex_pos = np.concatenate((vertex_pos, norm), axis=1)
    vertex_pos = vertex_pos[1:]
    vertex_pos = np.round(vertex_pos, 3)
    vertex_pos = vertex_pos.astype(np.float32)

    index = index[1:]
    index = index - 1

    vertex_pos = glm.array(vertex_pos.flatten())
    index = glm.array(index.flatten())    
    
    objectInfo.set_MAX(MAX)
    objectInfo.set_vertex_pos(vertex_pos)
    objectInfo.set_vertex_norm(vertex_norm)
    objectInfo.set_index(index)
    objectInfo.set_count(count)

def HierarchyParsing(objName, objectInfo):
    dir = os.getcwd()
    paths = os.path.join(dir, objName)
    objectInfo.set_paths(paths)
    parsingObject(paths, objectInfo)

def prepare_vao_object(objectInfo):
    vertices = objectInfo.get_vertex_pos()
    indices = objectInfo.get_index()

    VAO = glGenVertexArrays(1)
    glBindVertexArray(VAO)

    VBO = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, VBO)

    EBO = glGenBuffers(1)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)

    glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices.ptr, GL_STATIC_DRAW)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices.ptr, GL_STATIC_DRAW)

    # configure vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * glm.sizeof(glm.float32), None)
    glEnableVertexAttribArray(0)

    # configure vertex normals
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * glm.sizeof(glm.float32), ctypes.c_void_p(3*glm.sizeof(glm.float32)))
    glEnableVertexAttribArray(1)

    return VAO

def DrawObject(objectInfo, vao, node,  MVP, M, view_pos, change_light_pos, MVP_loc, M_loc, view_pos_loc, color_loc, change_light_pos_loc):
    indices = objectInfo.get_index()

    MVP = MVP * node.get_global_transform() * node.get_shape_transform()
    color = node.get_color()

    glBindVertexArray(vao)
    glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, glm.value_ptr(MVP))
    glUniformMatrix4fv(M_loc, 1, GL_FALSE, glm.value_ptr(M))
    glUniform3f(color_loc, color.r, color.g, color.b)
    glUniform3f(view_pos_loc, view_pos.x, view_pos.y, view_pos.z)
    glUniformMatrix4fv(change_light_pos_loc, 1, GL_FALSE, glm.value_ptr(change_light_pos))
    glDrawElements(GL_TRIANGLES, len(indices), GL_UNSIGNED_INT, None)

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
    global isDropped

    # initialize glfw
    if not glfwInit():
        return
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3)   # OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE)  # Do not allow legacy OpenGl API calls
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE) # for macOS

    # create a window and OpenGL context
    window = glfwCreateWindow(1000, 1000, 'Project #2. 2017029343 김기환', None, None)
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
    shader_for_plane = load_shaders(g_vertex_shader_src_color_attribute, g_fragment_shader_src_color_attribute)
    shader_for_object = load_shaders(g_vertex_shader_src_color_uniform, g_fragment_shader_src_color_uniform)
    
    # get uniform locations
    MVP_loc_plane = glGetUniformLocation(shader_for_plane, 'MVP')
    color_loc_plane = glGetUniformLocation(shader_for_plane, 'color')

    MVP_loc_object = glGetUniformLocation(shader_for_object, 'MVP')
    M_loc_object = glGetUniformLocation(shader_for_object, 'M')
    view_pos_loc_object = glGetUniformLocation(shader_for_object, 'view_pos')
    color_loc_object = glGetUniformLocation(shader_for_object, 'color')
    change_light_pos_loc_object = glGetUniformLocation(shader_for_object, 'change_light_pos')
    
    print("Parsing the obj file may take some time. Please wait.")

    # prepare vaos
    vao_frameX = prepare_vao_frameX()
    vao_frameZ = prepare_vao_frameZ()

    HierarchyParsing('F2C.obj', Girl)
    vao_girl = prepare_vao_object(Girl)

    HierarchyParsing('ShoppingCart.obj', ShoppingCart1)
    vao_shoppingCart1 = prepare_vao_object(ShoppingCart1)

    ShoppingCart2 = ShoppingCart1
    vao_shoppingCart2 = prepare_vao_object(ShoppingCart2)

    HierarchyParsing('Burger.obj', Burger)
    vao_burger = prepare_vao_object(Burger)

    HierarchyParsing('RoastChicken.obj', RoastChicken)
    vao_roastChicken = prepare_vao_object(RoastChicken)
    
    HierarchyParsing('Bottle.obj', Bottle)
    vao_bottle = prepare_vao_object(Bottle)

    HierarchyParsing('PlasticBox.obj', PlasticBox)
    vao_plasticBox = prepare_vao_object(PlasticBox)

    GirlNode = Node(None, glm.scale((.005, .005, .005)), glm.vec3(0.5, 0.4, 0.1))
    ShoppingCart1Node = Node(GirlNode, glm.scale((.01, .01, .01)), glm.vec3(0, 1, 0))
    ShoppingCart2Node = Node(GirlNode, glm.scale((.01, .01, .01)), glm.vec3(1, 0, 0))
    BurgerNode = Node(ShoppingCart1Node, glm.scale((.025, .025, .025)), glm.vec3(1, 1, 0))
    RoastChickenNode = Node(ShoppingCart1Node, glm.scale((.005, .005, .005)), glm.vec3(1, 1, 0))
    BottleNode = Node(ShoppingCart2Node, glm.scale((.25, .25, .25)), glm.vec3(1, 1, 0))
    PlasticBoxNode = Node(ShoppingCart2Node, glm.scale((.025, .025, .025)), glm.vec3(1, 1, 1))

    print("The parsing is complete. Now rendering the model in 3D space.")

    # loop until the user closes the window
    while not glfwWindowShouldClose(window):
        # render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glClearColor(.25, .25, .25, 0.0)
        glEnable(GL_DEPTH_TEST)

        glUseProgram(shader_for_plane)

        if (isPerspective == False):
            VP = renderOrtho() 
        elif (isPerspective == True):
            VP = renderPerspective()   

        M = glm.mat4()
        MVP = VP * M
        view_pos = CameraPosition
        
        DrawPlane(vao_frameX, vao_frameZ, MVP, MVP_loc_plane, color_loc_plane)

        t = glfwGetTime()
        th = np.radians(t * 60)
        change_light_pos = glm.rotate(th, glm.vec3(0, 1, 0))

        glUseProgram(shader_for_object)
        if (isDropped):
            vao_single = prepare_vao_object(Single)

            MAX = Single.get_MAX()
            SingleNode = Node(None, glm.scale(glm.mat4(), (1/MAX, 1/MAX, 1/MAX)), glm.vec3(1, 1, 1))
            isDropped = False

        if (isSingleMeshReady):
            DrawObject(Single, vao_single, SingleNode, MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)
            
        elif (isHierarchy):
            # set local transformations of each node
            GirlNode.update_tree_global_transform()

            GirlNode.set_transform(glm.translate(glm.mat4(), (np.cos(th), 0, 0)))
            ShoppingCart1Node.set_transform(glm.translate(glm.mat4(), (0, 0, np.cos(th))) * glm.translate(glm.mat4(), (1, 1, 1)))
            # ShoppingCart1Node.set_transform(glm.rotate(t, (1, 0, 0)) * glm.translate(glm.mat4(), (1, 1, 1)))
            ShoppingCart2Node.set_transform(glm.rotate(t, (-1, 0, 0)) * glm.translate(glm.mat4(), (-1, 1, -1)))
            BurgerNode.set_transform(glm.rotate(5 * t, (0, 1, 0)) * glm.translate(glm.mat4(), (.5, 0, .5)))
            RoastChickenNode.set_transform(glm.rotate(5 * t, (1, 0, 0)) * glm.translate(glm.mat4(), (-.5, 0, -.5)))
            BottleNode.set_transform(glm.rotate(5 * t, (0, 1, 0)) * glm.translate(glm.mat4(), (.5, 0, .5)))
            PlasticBoxNode.set_transform(glm.rotate(5 * t, (1, 0, 0)) * glm.translate(glm.mat4(), (-.5, 0, -.5)))
            
            DrawObject(Girl, vao_girl, GirlNode, MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)
            DrawObject(ShoppingCart1, vao_shoppingCart1, ShoppingCart1Node, MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)
            DrawObject(ShoppingCart2, vao_shoppingCart2, ShoppingCart2Node, MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)
            DrawObject(Burger, vao_burger, BurgerNode, MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)
            DrawObject(RoastChicken, vao_roastChicken, RoastChickenNode, MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)
            DrawObject(Bottle, vao_bottle, BottleNode, MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)
            DrawObject(PlasticBox, vao_plasticBox, PlasticBoxNode,  MVP, M, view_pos, change_light_pos, 
                       MVP_loc_object, M_loc_object, view_pos_loc_object, color_loc_object, change_light_pos_loc_object)

        # swap front and back buffers
        glfwSwapBuffers(window)

        # poll events
        glfwPollEvents()

    # terminate glfw
    glfwTerminate()

if __name__ == "__main__":
    main()
