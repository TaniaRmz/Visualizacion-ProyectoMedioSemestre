#ifndef MODELO_H
#define MODELO_H

#include <string>
#include <vector>

// Una posición en el espacio, con sus tres coordenadas.
struct Punto {
    float x, y, z;
};

// Reúne aquí los datos de un OBJ para pasarlos juntos a las funciones.
struct Modelo {
    std::string nombre;
    std::vector<Punto> vertices;
    // Cada grupo de tres índices representa un triángulo.
    std::vector<unsigned int> indices;
    // Son las esquinas de la caja que contiene al modelo.
    // Sirven para centrarlo y para saber su altura mínima y máxima.
    Punto minimo;
    Punto maximo;
};

// Estas declaraciones permiten usar las funciones de modelo.cpp desde otros archivos.
// leerOBJ devuelve true si logró cargar el modelo y false si encontró un problema.
bool leerOBJ(const std::string& ruta, Modelo& modelo);
void imprimirInformacion(const Modelo& modelo);

#endif
