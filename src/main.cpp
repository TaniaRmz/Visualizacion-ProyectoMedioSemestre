#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "modelo.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Este shader coloca cada vértice en la pantalla y pasa su altura al siguiente shader.
const char* vertexShaderSource = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 mvp;
    out float altura;
    void main() {
        gl_Position = mvp * vec4(aPos, 1.0);
        altura = aPos.y;
    }
)";

// Este shader decide el color de cada fragmento (un posible píxel del dibujo).
// Los ejes tienen un color fijo y el modelo usa un color según su altura original.
const char* fragmentShaderSource = R"(
    #version 410 core
    in float altura;
    out vec4 FragColor;
    uniform float alturaMinima;
    uniform float alturaMaxima;
    uniform bool esEje;
    uniform vec3 colorEje;
    void main() {
        if (esEje) {
            FragColor = vec4(colorEje, 1.0);
        } else {
            // t vale 0 abajo, 0.5 a media altura y 1 arriba.
            float rango = alturaMaxima - alturaMinima;
            // Si el modelo no tiene diferencia de altura, deja el color de en medio.
            float t = 0.5;
            if (rango > 0.0) t = clamp((altura - alturaMinima) / rango, 0.0, 1.0);
            vec3 azul = vec3(0.0, 0.0, 1.0);
            vec3 amarillo = vec3(1.0, 1.0, 0.0);
            vec3 rojo = vec3(1.0, 0.0, 0.0);
            vec3 color;
            // mix mezcla dos colores. Hace una mezcla para cada mitad.
            if (t <= 0.5) color = mix(azul, amarillo, t * 2.0);
            else color = mix(amarillo, rojo, (t - 0.5) * 2.0);
            FragColor = vec4(color, 1.0);
        }
    }
)";

// Manda el texto del shader a OpenGL y revisa si pudo compilarlo.
// Si hay un error, lo muestra en la terminal y devuelve 0.
GLuint compilarShader(GLenum tipo, const char* codigo) {
    GLuint shader = glCreateShader(tipo);
    glShaderSource(shader, 1, &codigo, nullptr);
    glCompileShader(shader);
    GLint correcto;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &correcto);
    if (!correcto) {
        char mensaje[2048];
        glGetShaderInfoLog(shader, sizeof(mensaje), nullptr, mensaje);
        std::cerr << "Error al compilar shader:\n" << mensaje << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Une el shader de posiciones y el de colores en un solo programa.
// Una vez unidos, borra los shaders separados porque el programa los conserva.
GLuint crearPrograma() {
    GLuint vertex = compilarShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragment = compilarShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!vertex || !fragment) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }
    GLuint programa = glCreateProgram();
    glAttachShader(programa, vertex);
    glAttachShader(programa, fragment);
    glLinkProgram(programa);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint correcto;
    glGetProgramiv(programa, GL_LINK_STATUS, &correcto);
    if (!correcto) {
        char mensaje[2048];
        glGetProgramInfoLog(programa, sizeof(mensaje), nullptr, mensaje);
        std::cerr << "Error al unir shaders:\n" << mensaje << '\n';
        glDeleteProgram(programa);
        return 0;
    }
    return programa;
}

// Usa tres enteros con nombres para recordar qué dibuja cada modo.
enum Modo { SUPERFICIE, PUNTOS, LINEAS };
const char* nombresModo[] = {"superficie", "puntos", "lineas"};

// Guarda las opciones que cambian al usar el teclado.
// La matriz identidad (1.0f) significa que todavía no hay ningún giro.
struct Estado {
    int modeloActual = 0;
    int modo = SUPERFICIE;
    bool cambiarModelo = false;
    bool detalleCompleto = false;
    glm::mat4 rotacion = glm::mat4(1.0f);
};

// El callback recibe cada pulsación una vez (mantener una flecha presionada
// no carga el mismo archivo muchas veces por segundo)
void teclado(GLFWwindow* ventana, int tecla, int, int accion, int) {
    if (accion != GLFW_PRESS) return;
    // Recupera el estado asociado a esta ventana al iniciar el programa.
    Estado& estado = *static_cast<Estado*>(glfwGetWindowUserPointer(ventana));
    if (tecla == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(ventana, true);
    if (tecla == GLFW_KEY_RIGHT || tecla == GLFW_KEY_LEFT) {
        int paso = tecla == GLFW_KEY_RIGHT ? 1 : -1;
        // El módulo hace que después del último modelo se vuelva al primero.
        // Suma 6 para que también funcione al retroceder desde el primero.
        estado.modeloActual = (estado.modeloActual + paso + 6) % 6;
        estado.cambiarModelo = true;
    }
    if (tecla == GLFW_KEY_1) estado.modo = PUNTOS;
    if (tecla == GLFW_KEY_2) estado.modo = LINEAS;
    if (tecla == GLFW_KEY_3) estado.modo = SUPERFICIE;
    if (tecla == GLFW_KEY_D) estado.detalleCompleto = !estado.detalleCompleto;
    if (tecla == GLFW_KEY_R) estado.rotacion = glm::mat4(1.0f);
}

// Copia el modelo de la memoria del programa a la memoria de OpenGL.
// El VAO guarda la configuración, el VBO las posiciones, y el EBO los índices.
void subirModelo(const Modelo& modelo, GLuint vao, GLuint vbo, GLuint ebo) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, modelo.vertices.size() * sizeof(Punto),
                 modelo.vertices.data(), GL_STATIC_DRAW);
    // EBO guarda los índices, evita copiar un vértice por cada cara que lo usa.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, modelo.indices.size() * sizeof(unsigned int),
                 modelo.indices.data(), GL_STATIC_DRAW);
    // Cada posición tiene tres números float seguidos: X, Y y Z.
    // El atributo 0 corresponde a aPos en el vertex shader.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Punto), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// Muestreo uniforme para que una malla muy densa siga siendo legible.
// La superficie y los datos originales siempre conservan todos sus triángulos.
std::vector<unsigned int> indicesVista(const Modelo& modelo, bool puntos) {
    std::vector<unsigned int> indices;
    std::size_t cantidad = puntos ? modelo.vertices.size() : modelo.indices.size() / 3;
    std::size_t limite = puntos ? 12000 : 18000;
    // Redondea hacia arriba para no rebasar el límite.
    // Se eligen por su orden en el archivo.
    std::size_t paso = (cantidad + limite - 1) / limite;
    for (std::size_t i = 0; i < cantidad; i += paso) {
        if (puntos) indices.push_back(static_cast<unsigned int>(i));
        // En líneas toma los tres índices de cada triángulo elegido.
        else indices.insert(indices.end(), modelo.indices.begin() + i * 3,
                            modelo.indices.begin() + i * 3 + 3);
    }
    return indices;
}

// Calcula una transformación para que cualquier modelo quede centrado y quepa.
// Solo cambia cómo se ve, las posiciones guardadas en el modelo siguen iguales.
glm::mat4 encuadrarModelo(const Modelo& modelo) {
    glm::vec3 minimo(modelo.minimo.x, modelo.minimo.y, modelo.minimo.z);
    glm::vec3 maximo(modelo.maximo.x, modelo.maximo.y, modelo.maximo.z);
    glm::vec3 centro = (minimo + maximo) * 0.5f;
    float radio = glm::length(maximo - minimo) * 0.5f;
    if (radio == 0.0f) radio = 1.0f;
    // Centra la caja en el origen y hace que su radio sea 1.
    // La escala es igual en X,Y,Z para no deformar el modelo.
    glm::mat4 ajuste = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / radio));
    return glm::translate(ajuste, -centro);
}

// Lee la imagen recién dibujada y la guarda como archivo PPM.
// Cada píxel ocupa tres bytes: rojo, verde y azul.
bool guardarCaptura(const std::string& ruta, int ancho, int alto) {
    std::vector<unsigned char> pixeles(ancho * alto * 3);
    // Evita que OpenGL añada espacio extra al final de las filas de píxeles.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, ancho, alto, GL_RGB, GL_UNSIGNED_BYTE, pixeles.data());
    std::ofstream archivo(ruta, std::ios::binary);
    if (!archivo) return false;
    archivo << "P6\n" << ancho << ' ' << alto << "\n255\n";
    // OpenGL empieza abajo y las imágenes PPM empiezan arriba.
    for (int y = alto - 1; y >= 0; y--) {
        archivo.write(reinterpret_cast<const char*>(pixeles.data() + y * ancho * 3), ancho * 3);
    }
    return static_cast<bool>(archivo);
}

int main(int argc, char** argv) {
    const std::vector<std::string> archivos = {
        "Teapot.obj", "bunny.obj", "car.obj", "casa.obj", "cat.obj", "pescado.obj"
    };
    // Estas opciones se escriben en la terminal después del nombre del programa.
    // Sin opciones se abre el visualizador normal.
    bool verificar = argc == 2 && std::string(argv[1]) == "--verificar";
    bool capturas = argc == 2 && std::string(argv[1]) == "--capturas";
    if (argc > 1 && !verificar && !capturas) {
        std::cerr << "Uso: visualizador [--verificar | --capturas]\n";
        return 1;
    }
    if (verificar) {
        // Revisa los seis archivos sin abrir una ventana.
        for (const std::string& nombre : archivos) {
            Modelo modelo;
            if (!leerOBJ(std::string(CARPETA_MODELOS) + "/" + nombre, modelo)) return 1;
            imprimirInformacion(modelo);
        }
        return 0;
    }

    // GLFW se encarga de la ventana y de recibir el teclado.
    // Pide OpenGL 4.1 porque es la versión que usa el programa en los shaders.
    if (!glfwInit()) {
        std::cerr << "No se pudo iniciar GLFW. Ejecuta desde una sesion grafica de macOS.\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Requiere un contexto compatible hacia adelante para OpenGL moderno.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow* ventana = glfwCreateWindow(900, 700, "Visualizador OBJ", nullptr, nullptr);
    if (!ventana) {
        std::cerr << "No se pudo crear la ventana OpenGL 4.1.\n";
        glfwTerminate();
        return 1;
    }
    // Elige esta ventana para dibujar. Después GLEW prepara las funciones de OpenGL.
    glfwMakeContextCurrent(ventana);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "No se pudo iniciar GLEW.\n";
        glfwDestroyWindow(ventana);
        glfwTerminate();
        return 1;
    }
    // GLEW puede dejar este error al consultar extensiones en un contexto core.
    while (glGetError() != GL_NO_ERROR) {}
    // Intenta sincronizar el dibujo con la actualización de la pantalla.
    glfwSwapInterval(1);
    GLuint programa = crearPrograma();
    if (!programa) {
        glfwDestroyWindow(ventana);
        glfwTerminate();
        return 1;
    }

    // Asocia las opciones a la ventana y registra la función del teclado.
    Estado estado;
    glfwSetWindowUserPointer(ventana, &estado);
    glfwSetKeyCallback(ventana, teclado);
    // Reserva identificadores para los datos del modelo completo y las vistas reducidas.
    GLuint vao, vbo, ebo;
    GLuint eboPuntos, eboLineas;
    GLsizei cantidadPuntos = 0, cantidadLineas = 0;
    glGenBuffers(1, &eboPuntos);
    glGenBuffers(1, &eboLineas);
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    // Los tres segmentos que cruzan el origen: X rojo, Y verde y Z azul claro.
    float ejes[] = {
        -1.3f,0,0, 1.3f,0,0,
        0,-1.3f,0, 0,1.3f,0,
        0,0,-1.3f, 0,0,1.3f
    };
    GLuint vaoEjes, vboEjes;
    glGenVertexArrays(1, &vaoEjes);
    glGenBuffers(1, &vboEjes);
    glBindVertexArray(vaoEjes);
    glBindBuffer(GL_ARRAY_BUFFER, vboEjes);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ejes), ejes, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // La profundidad permite decidir qué parte queda delante cuando se superponen.
    glEnable(GL_DEPTH_TEST);
    glPointSize(1.0f);
    Modelo modelo;
    glm::mat4 ajuste(1.0f);
    estado.cambiarModelo = true;
    // -1 indica que todavía no se cargó ningún modelo correctamente.
    int modeloCargado = -1;
    int resultado = 0;
    int numeroCaptura = 0;
    if (capturas) std::filesystem::create_directories("imagenes");
    double tiempoAnterior = glfwGetTime();
    std::cout << "Flechas izquierda/derecha: cambiar modelo\n"
              << "1: puntos | 2: lineas | 3: superficie\n"
              << "Mantener X, Y o Z: rotar sobre ese eje\n"
              << "Shift: invertir giro | R: reiniciar giro | Esc: salir\n"
              << "D: alternar vista legible / detalle completo en puntos y lineas\n"
              << "Ejes: X rojo, Y verde, Z azul claro\n";

    // Cada vuelta dibuja una imagen nueva y atiende lo que hizo el usuario.
    while (!glfwWindowShouldClose(ventana)) {
        // Procesa eventos pendientes, como pulsaciones o cerrar la ventana.
        glfwPollEvents();
        if (estado.cambiarModelo) {
            std::cout << "\nLeyendo " << archivos[estado.modeloActual] << "...\n";
            if (!leerOBJ(std::string(CARPETA_MODELOS) + "/" + archivos[estado.modeloActual], modelo)) {
                // Si ya había un modelo válido, lo conserva cuando falla la nueva carga.
                if (modeloCargado < 0 || capturas) { resultado = 1; break; }
                estado.modeloActual = modeloCargado;
            } else {
                subirModelo(modelo, vao, vbo, ebo);
                glBindVertexArray(vao);
                // Prepara las listas reducidas una sola vez al cargar el modelo.
                // Al pulsar D solo elige qué lista dibujar, sin volver a leer el OBJ.
                auto puntos = indicesVista(modelo, true);
                auto lineas = indicesVista(modelo, false);
                cantidadPuntos = static_cast<GLsizei>(puntos.size());
                cantidadLineas = static_cast<GLsizei>(lineas.size());
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboPuntos);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, puntos.size() * sizeof(unsigned int), puntos.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboLineas);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, lineas.size() * sizeof(unsigned int), lineas.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                glBindVertexArray(0);
                ajuste = encuadrarModelo(modelo);
                estado.rotacion = glm::mat4(1.0f);
                modeloCargado = estado.modeloActual;
                imprimirInformacion(modelo);
            }
            estado.cambiarModelo = false;
            // Reinicia el reloj para que el tiempo de carga no se convierta en un giro grande.
            // La lectura puede tardar
            tiempoAnterior = glfwGetTime();
        }

        // El giro depende del tiempo transcurrido para que no cambie de velocidad
        // según cuántas imágenes por segundo alcance la computadora.
        double ahora = glfwGetTime();
        float segundos = static_cast<float>(ahora - tiempoAnterior);
        tiempoAnterior = ahora;
        // Limita el tiempo a 0.1 segundos para evitar saltos si una vuelta tarda mucho.
        float angulo = glm::radians(60.0f) * std::min(segundos, 0.1f);
        if (glfwGetKey(ventana, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(ventana, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) angulo = -angulo;
        // Multiplicar por la izquierda hace que el giro use los ejes fijos de la escena.
        if (glfwGetKey(ventana, GLFW_KEY_X) == GLFW_PRESS)
            estado.rotacion = glm::rotate(glm::mat4(1.0f), angulo, glm::vec3(1,0,0)) * estado.rotacion;
        if (glfwGetKey(ventana, GLFW_KEY_Y) == GLFW_PRESS)
            estado.rotacion = glm::rotate(glm::mat4(1.0f), angulo, glm::vec3(0,1,0)) * estado.rotacion;
        if (glfwGetKey(ventana, GLFW_KEY_Z) == GLFW_PRESS)
            estado.rotacion = glm::rotate(glm::mat4(1.0f), angulo, glm::vec3(0,0,1)) * estado.rotacion;

        // Usa el tamaño real del área de dibujo, que puede cambiar al redimensionar.
        // Si está minimizada y no tiene área, espera antes de seguir.
        int ancho, alto;
        glfwGetFramebufferSize(ventana, &ancho, &alto);
        if (ancho == 0 || alto == 0) { glfwWaitEvents(); continue; }
        glViewport(0, 0, ancho, alto);
        // Borra el color y la profundidad anteriores para empezar una imagen limpia.
        glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float proporcion = static_cast<float>(ancho) / alto;
        // Si la ventana es angosta, aleja la cámara para que el modelo quepa.
        float distancia = 4.2f / std::min(proporcion, 1.0f);
        // La cámara mira desde una posición inclinada hacia el centro de la escena.
        glm::mat4 vista = glm::lookAt(glm::normalize(glm::vec3(1.5f,1.0f,3.0f)) * distancia,
                                     glm::vec3(0), glm::vec3(0,1,0));
        // La perspectiva hace que lo lejano se vea más pequeño.
        // Conserva la proporción de la ventana para no estirar el objeto.
        glm::mat4 proyeccion = glm::perspective(glm::radians(45.0f), proporcion, 0.1f, distancia + 10.0f);
        // Primero ajusta y gira el modelo, luego lo ubica respecto a la cámara
        // y al final lo proyecta a la pantalla. Las matrices actúan de derecha a izquierda.
        glm::mat4 mvp = proyeccion * vista * estado.rotacion * ajuste;
        glUseProgram(programa);
        // Los uniform son valores que el programa envía a los shaders para todo este dibujo.
        glUniformMatrix4fv(glGetUniformLocation(programa, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1f(glGetUniformLocation(programa, "alturaMinima"), modelo.minimo.y);
        glUniform1f(glGetUniformLocation(programa, "alturaMaxima"), modelo.maximo.y);
        glUniform1i(glGetUniformLocation(programa, "esEje"), false);
        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        if (estado.modo != SUPERFICIE) {
            // Primero guarda la profundidad de toda la superficie sin colorearla.
            // Así los puntos y las aristas de atrás no saturan la vista.
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glEnable(GL_POLYGON_OFFSET_FILL);
            // Aparta un poco la profundidad del relleno para que no tape
            // los puntos y líneas de su propia superficie por diferencias de redondeo.
            glPolygonOffset(1.0f, 1.0f);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(modelo.indices.size()), GL_UNSIGNED_INT, nullptr);
            glDisable(GL_POLYGON_OFFSET_FILL);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            // Permite dibujar también a la misma profundidad que la superficie.
            glDepthFunc(GL_LEQUAL);
        }
        if (estado.modo == PUNTOS) {
            if (estado.detalleCompleto) {
                glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(modelo.vertices.size()));
            } else {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboPuntos);
                glDrawElements(GL_POINTS, cantidadPuntos, GL_UNSIGNED_INT, nullptr);
            }
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, estado.modo == LINEAS ? GL_LINE : GL_FILL);
            bool simplificar = estado.modo == LINEAS && !estado.detalleCompleto;
            if (simplificar) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboLineas);
            glDrawElements(GL_TRIANGLES, simplificar ? cantidadLineas : static_cast<GLsizei>(modelo.indices.size()), GL_UNSIGNED_INT, nullptr);
        }
        // Deja las opciones de dibujo listas para la siguiente parte de la escena.
        glDepthFunc(GL_LESS);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Los ejes no llevan el ajuste ni el giro del modelo: quedan fijos en la escena.
        mvp = proyeccion * vista;
        glUniformMatrix4fv(glGetUniformLocation(programa, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1i(glGetUniformLocation(programa, "esEje"), true);
        glBindVertexArray(vaoEjes);
        // Dibuja los ejes encima para que el origen también se vea dentro del modelo.
        glDisable(GL_DEPTH_TEST);
        const glm::vec3 colores[] = {{1,0.25f,0.25f}, {0.2f,1,0.3f}, {0.3f,0.7f,1}};
        for (int i = 0; i < 3; i++) {
            glUniform3fv(glGetUniformLocation(programa, "colorEje"), 1, glm::value_ptr(colores[i]));
            glDrawArrays(GL_LINES, i * 2, 2);
        }
        glEnable(GL_DEPTH_TEST);
        std::string titulo = modelo.nombre + " | " + nombresModo[estado.modo] + " | 1/2/3 modos | flechas modelos | X/Y/Z giro";
        if (estado.modo != SUPERFICIE) titulo += estado.detalleCompleto ? " | D: detalle completo" : " | D: vista legible";
        glfwSetWindowTitle(ventana, titulo.c_str());

        // Si OpenGL reporta un error, lo muestra para poder encontrar el problema.
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "Error OpenGL: " << error << '\n';
            resultado = 1;
            break;
        }
        // En modo capturas guarda una imagen por modo y luego pasa al otro modelo.
        // El contador recorre superficie, puntos y líneas con el resto de dividir entre 3.
        if (capturas) {
            std::string ruta = "imagenes/" + modelo.nombre.substr(0, modelo.nombre.size() - 4)
                             + "_" + nombresModo[estado.modo] + ".ppm";
            if (!guardarCaptura(ruta, ancho, alto)) { resultado = 1; break; }
            numeroCaptura++;
            estado.modo = numeroCaptura % 3;
            if (numeroCaptura % 3 == 0) {
                estado.modeloActual++;
                if (estado.modeloActual == static_cast<int>(archivos.size())) break;
                estado.cambiarModelo = true;
            }
        }
        // Muestra la imagen terminada. La siguiente se prepara en otro búfer
        // para que no se vea cómo se va dibujando por partes.
        glfwSwapBuffers(ventana);
    }

    // Al salir libera los recursos de OpenGL y cierra la ventana.
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &eboPuntos);
    glDeleteBuffers(1, &eboLineas);
    glDeleteVertexArrays(1, &vaoEjes);
    glDeleteBuffers(1, &vboEjes);
    glDeleteProgram(programa);
    glfwDestroyWindow(ventana);
    glfwTerminate();
    return resultado;
}
