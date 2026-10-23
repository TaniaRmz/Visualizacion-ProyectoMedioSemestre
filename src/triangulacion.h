#ifndef TRIANGULACION_H
#define TRIANGULACION_H

#include "modelo.h"
#include <algorithm>
#include <cmath>

// Para revisar una cara es más sencillo trabajar en un plano con dos coordenadas.
struct Punto2D { double x, y; };

// El signo indica de qué lado del segmento AB está C.
inline double lado(Punto2D a, Punto2D b, Punto2D c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// Divide una cara en triángulos recortando una esquina válida a la vez.
// cara se recibe como copia porque se le quitan vértices durante el proceso.
// inline permite definir estas funciones en este encabezado e incluirlo en varios archivos.
inline bool triangularCara(std::vector<unsigned int> cara, const std::vector<Punto>& vertices,
                           std::vector<unsigned int>& indices) {
    // Si ya es un triángulo, no hay que dividir nada.
    if (cara.size() == 3) {
        indices.insert(indices.end(), cara.begin(), cara.end());
        return true;
    }
    // Para las caras con entrantes, como algunas del gato, recorta esquinas.
    // Primero elige el plano XY, XZ o YZ donde la cara se ve más de frente.
    // Calcula una dirección perpendicular a la cara para elegir el plano.
    // Quita la coordenada donde esa dirección tiene mayor valor absoluto.
    double normal[3] = {0, 0, 0};
    for (std::size_t i = 0; i < cara.size(); i++) {
        Punto a = vertices[cara[i]], b = vertices[cara[(i + 1) % cara.size()]];
        normal[0] += (double(a.y) - b.y) * (double(a.z) + b.z);
        normal[1] += (double(a.z) - b.z) * (double(a.x) + b.x);
        normal[2] += (double(a.x) - b.x) * (double(a.y) + b.y);
    }
    int quitar = 0;
    for (int i = 1; i < 3; i++)
        if (std::abs(normal[i]) > std::abs(normal[quitar])) quitar = i;

    // Proyecta la cara al plano elegido conservando el orden de sus vértices.
    std::vector<Punto2D> puntos;
    for (unsigned int indice : cara) {
        Punto p = vertices[indice];
        if (quitar == 0) puntos.push_back({p.y, p.z});
        if (quitar == 1) puntos.push_back({p.x, p.z});
        if (quitar == 2) puntos.push_back({p.x, p.y});
    }
    // El signo del área indica en qué sentido se recorre el borde.
    // Así las pruebas funcionan tanto en sentido horario como antihorario.
    double area = 0;
    for (std::size_t i = 0; i < puntos.size(); i++) {
        Punto2D a = puntos[i], b = puntos[(i + 1) % puntos.size()];
        area += a.x * b.y - a.y * b.x;
    }
    double sentido = area >= 0 ? 1 : -1;
    // Busca una esquina que sobresalga y cuyo triángulo no contenga otros vértices.
    while (cara.size() > 3) {
        bool encontrada = false;
        for (std::size_t i = 0; i < cara.size(); i++) {
            // El módulo conecta el último vértice con el primero para cerrar el borde.
            std::size_t anterior = (i + cara.size() - 1) % cara.size();
            std::size_t siguiente = (i + 1) % cara.size();
            Punto2D a = puntos[anterior], b = puntos[i], c = puntos[siguiente];
            // Descarta las esquinas que se meten hacia dentro o están en línea recta.
            if (lado(a, b, c) * sentido <= 0) continue;
            bool contieneOtro = false;
            for (std::size_t j = 0; j < cara.size(); j++) {
                if (j == anterior || j == i || j == siguiente) continue;
                Punto2D p = puntos[j];
                // Si el punto queda del lado interior de los tres bordes,
                // está dentro del triángulo o sobre su borde: no recorta esa esquina.
                if (lado(a,b,p) * sentido >= 0 && lado(b,c,p) * sentido >= 0 &&
                    lado(c,a,p) * sentido >= 0) {
                    contieneOtro = true;
                    break;
                }
            }
            if (contieneOtro) continue;
            // Guarda el triángulo y quita su esquina de las dos listas.
            // Vuelve a buscar desde el inicio porque cambió la forma de la cara.
            indices.insert(indices.end(), {cara[anterior], cara[i], cara[siguiente]});
            cara.erase(cara.begin() + i);
            puntos.erase(puntos.begin() + i);
            encontrada = true;
            break;
        }
        // Un polígono que se cruza consigo mismo no tiene una división simple.
        if (!encontrada) return false;
    }
    // Al final solo quedan tres vértices: guarda el último triángulo.
    indices.insert(indices.end(), cara.begin(), cara.end());
    return true;
}

#endif
