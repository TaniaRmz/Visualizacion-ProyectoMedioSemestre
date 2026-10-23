#include "modelo.h"
#include "triangulacion.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

// Abre el OBJ como texto y lee sus registros línea por línea.
bool leerOBJ(const std::string& ruta, Modelo& modelo) {
    std::ifstream archivo(ruta);
    if (!archivo) {
        std::cerr << "No se pudo abrir: " << ruta << '\n';
        return false;
    }

    // Lee en otra variable para no perder el modelo actual si hay un error.
    Modelo nuevo;
    nuevo.nombre = ruta.substr(ruta.find_last_of("/\\") + 1);
    std::string linea;
    int numeroLinea = 0;

    try {
        while (std::getline(archivo, linea)) {
            numeroLinea++;
            std::istringstream datos(linea);
            std::string tipo;
            // La primera palabra indica el dato: v es vértice y f es cara.
            datos >> tipo;

            if (tipo == "v") {
                Punto p;
                // Revisa que haya tres coordenadas válidas antes de guardar el punto.
                if (!(datos >> p.x >> p.y >> p.z) ||
                    !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
                    throw std::runtime_error("vertice incorrecto");
                }
                // El primer punto inicia la caja. Los demás la amplían si hace falta.
                if (nuevo.vertices.empty()) {
                    nuevo.minimo = nuevo.maximo = p;
                } else {
                    nuevo.minimo.x = std::min(nuevo.minimo.x, p.x);
                    nuevo.minimo.y = std::min(nuevo.minimo.y, p.y);
                    nuevo.minimo.z = std::min(nuevo.minimo.z, p.z);
                    nuevo.maximo.x = std::max(nuevo.maximo.x, p.x);
                    nuevo.maximo.y = std::max(nuevo.maximo.y, p.y);
                    nuevo.maximo.z = std::max(nuevo.maximo.z, p.z);
                }
                nuevo.vertices.push_back(p);
            } else if (tipo == "f") {
                // Esta lista guarda los vértices que forman una cara, en su orden.
                std::vector<unsigned int> cara;
                std::string dato;
                while (datos >> dato) {
                    if (dato[0] == '#') break;
                    // No usa texturas ni normales porque el color depende de Y.
                    // Se queda con lo que va antes de la primera diagonal.
                    // Por ejemplo, de 15/2/7 solo necesita el 15.
                    std::string textoIndice = dato.substr(0, dato.find('/'));
                    std::size_t leidos;
                    long long indice = std::stoll(textoIndice, &leidos);
                    // No acepta texto sobrante ni el índice 0, que no existe en OBJ.
                    if (leidos != textoIndice.size() || indice == 0) {
                        throw std::runtime_error("indice incorrecto");
                    }
                    // En OBJ se cuenta desde 1. Un índice negativo cuenta desde el final.
                    long long posicion = indice > 0 ? indice - 1 :
                        static_cast<long long>(nuevo.vertices.size()) + indice;
                    if (posicion < 0 || posicion >= static_cast<long long>(nuevo.vertices.size())) {
                        throw std::runtime_error("indice fuera de los vertices disponibles");
                    }
                    cara.push_back(static_cast<unsigned int>(posicion));
                }
                if (cara.size() < 3) throw std::runtime_error("cara con menos de tres vertices");

                // Convierte la cara en triángulos y añade sus índices al modelo.
                if (!triangularCara(cara, nuevo.vertices, nuevo.indices))
                    throw std::runtime_error("no se pudo dividir la cara en triangulos");
            }
            // Ignora comentarios, materiales, UV, normales y otros registros.
        }
        if (nuevo.vertices.empty() || nuevo.indices.empty()) {
            throw std::runtime_error("el archivo no tiene vertices y caras para dibujar");
        }
        // OpenGL recibe cantidades de tipo entero en las llamadas que usa el programa.
        // Comprueba que el tamaño se pueda representar sin desbordarse.
        if (nuevo.vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            nuevo.indices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("modelo demasiado grande para esta implementacion");
        }
    // Los errores de lectura llegan aquí. Incluye la línea para ubicar el problema.
    } catch (const std::exception& error) {
        std::cerr << nuevo.nombre << ", linea " << numeroLinea << ": " << error.what() << '\n';
        return false;
    }
    // Solo reemplaza el modelo anterior cuando terminó bien toda la lectura.
    // move transfiere las listas sin hacer otra copia de todos sus datos.
    modelo = std::move(nuevo);
    return true;
}

// Muestra un resumen en la terminal. Tres índices forman un triángulo.
void imprimirInformacion(const Modelo& modelo) {
    std::cout << "\nModelo: " << modelo.nombre
              << "\nVertices: " << modelo.vertices.size()
              << "\nTriangulos: " << modelo.indices.size() / 3
              << "\nMinimo (X,Y,Z): (" << modelo.minimo.x << ", "
              << modelo.minimo.y << ", " << modelo.minimo.z << ")"
              << "\nMaximo (X,Y,Z): (" << modelo.maximo.x << ", "
              << modelo.maximo.y << ", " << modelo.maximo.z << ")\n";
}
