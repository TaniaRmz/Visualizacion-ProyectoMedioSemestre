#!/bin/bash
# Entra a la carpeta del proyecto aunque se ejecute desde otra ubicación.
set -euo pipefail
cd "$(dirname "$0")"

# Compila la versión actual antes de tomar las imágenes.
cmake -B build
cmake --build build

# Abre el visualizador, recorre los seis modelos y guarda los tres modos.
./build/visualizador --capturas

# Convierte las 18 capturas a PNG para que se puedan ver en el README.
for modelo in Teapot bunny car casa cat pescado; do
    for modo in superficie puntos lineas; do
        archivo="imagenes/${modelo}_${modo}"
        sips -s format png "${archivo}.ppm" --out "${archivo}.png" >/dev/null
        # Quita el PPM intermedio solo después de convertirlo correctamente.
        rm "${archivo}.ppm"
    done
done
printf 'Listo: se actualizaron las 18 capturas en imagenes/.\n'
