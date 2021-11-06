# FFT_FRDMK64F
---

En este repositorio se encuentran los códigos fuentes referidos al proyecto de cálculo de la FFT de una señal, de la materia *"Procesamiento Digital de Señales"* de la *Facultad de Ciencias Exactas Físicas y Naturales - Universidad Nacional de Córdoba*.

El entorno de desarrollo que se utilizó fue el MCUxpresso con el SDK de la placa FRDM-K64F.

El programa le pide al usuario que configure la cantidad de puntos de la FFT (opciones
disponibles de 512, 1024 y 2048 muestras) y en base a la selección se asignan punteros a 
los buffers correspondientes y se configuran los límites para luego recorrerlos.

Utilizando los botones de la placa se puede activar/desactivar el cómputo de la FFT (con el SW2) y con el SW1 se puede cambiar la velocidad de muestreo (8 [k/s], 16 [k/s], 22 [k/s], 44 [k/s] o 48 [k/s]).

Durante la ejecución del programa, cada vez que haya una nueva muestra del ADC, se utilizará el *input_buffer_ptr* 
para guardar la muestra en el buffer correspondiente. Si está activada la FFT se sacará por DAC lo que haya guardado
en el buffer de salida, señalado por el puntero *buffer_output_ptr*. Si no está activada la FFT se mostrará por DAC la última muestra tomada por el ADC.

Una vez lleno los 

Si se desactiva la FFT, se reestablece la posicion de los punteros para que los buffers tengan muestras consecutivas.
