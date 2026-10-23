# Proyecto de medio semestre: visualizador OBJ

**Tania Ramírez Plascencia**  
Visualización, Facultad de Ciencias, UNAM. Semestre 2027-1

Programa en C++ que abre los seis modelos proporcionados y permite verlos como puntos, líneas o superficie. Usa OpenGL 4.1, GLFW, GLEW y GLM.

## Compilar y ejecutar en macOS

Se necesita Homebrew y las herramientas de compilación de Apple. 
Si no están instaladas:

```bash
xcode-select --install
```

Instalar las bibliotecas y CMake (si ya están instalados, no es necesario repetirlo):

```bash
brew install cmake glfw glew glm
```

Abrir Terminal en la carpeta del proyecto y ejecutar:
```bash
cmake -B build
cmake --build build
./build/visualizador
```

La ventana comienza con la tetera en modo superficie. Hay que hacer clic en ella para que reciba el teclado. La información de cada modelo aparece en la terminal desde donde se abrió el programa.

Los modelos se buscan en la carpeta donde estaba el código al compilar. Si se mueve o descarga el proyecto en otra ubicación, hay que volver a compilarlo.

Si CMake no encuentra las bibliotecas de Homebrew:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build
```

## Controles

| Tecla | Acción |
| --- | --- |
| Flecha derecha | Siguiente modelo |
| Flecha izquierda | Modelo anterior |
| 1 | Nube de puntos |
| 2 | Líneas de los triángulos |
| 3 | Superficie |
| Mantener X | Girar alrededor del eje X |
| Mantener Y | Girar alrededor del eje Y |
| Mantener Z | Girar alrededor del eje Z |
| Shift junto con X, Y o Z | Girar en sentido contrario |
| D | Alternar vista legible y detalle completo en puntos y líneas |
| R | Regresar a la orientación inicial |
| Esc | Cerrar |

El orden es tetera, conejo, coche, casa, gato y pescado. Cambiar de modelo reinicia la rotación y conserva el modo de dibujo.

Los ejes se cruzan en el origen de la escena: **X rojo, Y verde y Z azul claro**. Se dibujan encima del modelo para que el origen no quede oculto. La cámara permanece fija, se puede cambiar el tamaño de la ventana.

## Cómo está organizado

```text
ProyectoMedioSemestre/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── modelo.h
│   ├── modelo.cpp
│   └── triangulacion.h
├── imagenes/
├── Teapot.obj
├── bunny.obj
├── car.obj
├── casa.obj
├── cat.obj
└── pescado.obj
```

- `main.cpp`: crea la ventana, compila los shaders, carga los buffers, procesa el teclado y dibuja.
- `modelo.h` y `modelo.cpp`: guardan los vértices y las caras, leen los OBJ y calculan mínimos y máximos.
- `triangulacion.h`: divide las caras con más de tres vértices en triángulos. Se separó porque el gato tiene polígonos más complicados.

## Explicación del programa

### Leer los modelos

Las líneas que empiezan con `v` guardan posiciones X, Y y Z. Las que empiezan con `f` indican qué vértices forman una cara. OBJ cuenta desde 1 y C++ desde 0, por eso restamos uno a los índices positivos. También se admiten índices negativos, que cuentan desde el último vértice leído.

En una cara puede aparecer `15`, `15/2/7` o `15//7`. En los tres casos usamos el primer número para buscar la posición. Las texturas, los materiales y las normales no se necesitan para el mapa de altura. Por eso el programa puede abrir los modelos aunque no estén sus archivos `.mtl`.

OpenGL dibuja las superficies como triángulos. Para las caras con más lados, buscamos una esquina que forme un triángulo sin otros puntos de la cara dentro. Guardamos ese triángulo, quitamos la esquina y repetimos. Esto permite manejar las caras con entrantes del gato. Se supone que las caras son polígonos simples, sin cruces entre sus lados.
Los registros `l` se ignoran: el modo líneas muestra los bordes de los triángulos de las caras.

Los vértices se guardan una sola vez en un VBO. El EBO contiene sus índices, que indican cómo formar los triángulos. El VAO guarda cómo debe interpretar OpenGL esos datos.

### Encuadrar cada objeto

Mientras se leen los vértices se busca el mínimo y máximo de cada coordenada. Estos dos puntos describen la caja que contiene el modelo.

El centro es `(minimo + maximo) / 2`. 
El radio es la mitad de la longitud de la diagonal de la caja. 
Primero se traslada el modelo para que su centro quede en el origen y después se aplica una escala uniforme `1 / radio`, así todos caben en una esfera de radio 1 y no se deforman.

Los ejes muestran el origen de la escena después de centrar el objeto. Los datos impresos en terminal y la altura para el color siguen usando las coordenadas originales del archivo. El objeto conserva sus colores cuando gira.

GLM calcula las matrices. La matriz de modelo centra, escala y gira el objeto; la de vista representa la cámara y la de proyección da la perspectiva. El vertex shader multiplica cada posición por `proyeccion * vista * modelo`.

### Colores por altura

Se usa Y como altura. El shader calcula:

```text
t = (altura - alturaMinima) / (alturaMaxima - alturaMinima)
```

- Si `t = 0`, el color es azul.
- Si `t = 0.5`, el color es amarillo.
- Si `t = 1`, el color es rojo.

Entre esos valores se usa `mix` para transicionar entre colores.
El punto medio es la mitad entre la altura mínima y máxima.
Si todas las alturas son iguales se usa amarillo (para evitar dividir entre cero).
El fragment shader calcula el color a partir de la altura interpolada entre los vértices.

### Formas de dibujar

En la vista legible se muestran hasta 12,000 vértices y hasta 18,000 triángulos muestreados uniformemente para las líneas. Esto permite distinguir los modos en modelos densos como el pescado; no modifica el OBJ ni la superficie. Pulsa **D** para mostrar todos los vértices y aristas. En puntos y líneas se calcula primero la profundidad de toda la superficie para ocultar las partes traseras.

La nube completa usa `glDrawArrays(GL_POINTS)` para dibujar cada vértice del archivo. Las líneas y la superficie usan `glDrawElements(GL_TRIANGLES)` con los mismos índices, como cambia `glPolygonMode` entre `GL_LINE` y `GL_FILL` se ven las diagonales que se añadieron al dividir los polígonos.

La prueba de profundidad hace que las partes cercanas oculten las lejanas.

## Imágenes del visualizador

Capturas que se tomaron ejecutando el programa.

### Los tres modos con la tetera

**Superficie**

![Tetera como superficie](imagenes/Teapot_superficie.png)

**Nube de puntos**

![Tetera como puntos](imagenes/Teapot_puntos.png)

**Líneas**

![Tetera como líneas](imagenes/Teapot_lineas.png)

### Otros modelos

| Modelo | Superficie | Puntos | Líneas |
| --- | --- | --- | --- |
| Conejo | [Ver](imagenes/bunny_superficie.png) | [Ver](imagenes/bunny_puntos.png) | [Ver](imagenes/bunny_lineas.png) |
| Coche | [Ver](imagenes/car_superficie.png) | [Ver](imagenes/car_puntos.png) | [Ver](imagenes/car_lineas.png) |
| Casa | [Ver](imagenes/casa_superficie.png) | [Ver](imagenes/casa_puntos.png) | [Ver](imagenes/casa_lineas.png) |
| Gato | [Ver](imagenes/cat_superficie.png) | [Ver](imagenes/cat_puntos.png) | [Ver](imagenes/cat_lineas.png) |
| Pescado | [Ver](imagenes/pescado_superficie.png) | [Ver](imagenes/pescado_puntos.png) | [Ver](imagenes/pescado_lineas.png) |

## Verificación

Para leer los seis OBJ e imprimir sus datos sin abrir una ventana:

```bash
./build/visualizador --verificar
```

| Modelo | Vértices del archivo | Triángulos después de dividir caras |
| --- | ---: | ---: |
| Teapot.obj | 3,644 | 6,320 |
| bunny.obj | 34,817 | 69,630 |
| car.obj | 249,481 | 499,257 |
| casa.obj | 1,327 | 2,665 |
| cat.obj | 6,570 | 13,056 |
| pescado.obj | 330,029 | 660,046 |

Para regenerar las 18 imágenes con la versión actual, ejecutar en Terminal desde la carpeta del proyecto:

```bash
bash actualizar_capturas.sh
```

El script compila el programa, abre la ventana, recorre los seis modelos en los tres modos y convierte las capturas a PNG. La ventana se cierra sola. Las imágenes de puntos y líneas usan la vista legible predeterminada; las de superficie conservan todos los triángulos.

## Referencias

- [Transformaciones con GLM](https://glm.g-truc.net/0.9.9/api/a00247.html).
- [Documentación de GLFW](https://www.glfw.org/documentation.html).
- [Documentación de GLEW](https://glew.sourceforge.net/).

Se utilizó apoyo de ChatGPT para revisar el código.
