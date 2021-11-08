# FFT_FRDMK64F

En este repositorio se encuentran los códigos fuentes referidos al proyecto de cálculo de la FFT de una señal, de la materia *"Procesamiento Digital de Señales"* de la *Facultad de Ciencias Exactas Físicas y Naturales - Universidad Nacional de Córdoba*.

In this repo you will find the source codes of the FFT calculation program in the context of the Electronics Engineering subject *"Digital Signal Processing"* of the *Facultad de Ciencias Exactas Físicas y Naturales - Universidad Nacional de Córdoba*.

---

## Índice / Index
1. [Español](#español).
2. [English](#english).

### Español

El entorno de desarrollo que se utilizó fue el MCUxpresso con el SDK de la placa FRDM-K64F.

El programa le pide al usuario que configure (via terminal) la cantidad de puntos de la FFT (opciones
disponibles de 512, 1024 y 2048 muestras) y en base a la selección se asignan punteros a 
los buffers correspondientes y se configuran los límites para luego recorrerlos.

Utilizando los botones de la placa se puede activar/desactivar el cómputo de la FFT (con el SW2) 
y con el SW1 se puede cambiar la velocidad de muestreo (8 [k/s], 16 [k/s], 22 [k/s], 44 [k/s] o 48 [k/s]).

Durante la ejecución del programa, cada vez que haya una nueva muestra del ADC, se utilizará el *input_buffer_ptr* 
para guardar la muestra en el buffer correspondiente. Si está activada la FFT se sacará por el DAC el resultado que está almacenado
en el buffer de salida, señalado por el puntero *buffer_output_ptr*. Si no está activada la FFT se mostrará por el DAC la última muestra tomada por el ADC.

Una vez lleno el buffer de entrada, se computa la FFT y se hace la operación de upscaling que indica la [documentación oficial CMSIS](https://arm-software.github.io/CMSIS_5/DSP/html/group__RealFFT.html) teniendo en cuenta la cantidad de puntos de la FFT.

Si se desactiva la FFT, se restablece la posicion de los punteros para que los buffers tengan muestras consecutivas.

### English

The development environment used was the MCUxpresso with the FRDM-K64F board SDK. 

The program asks the user to configure (via terminal) the number of points of the FFT (options
512, 1024 and 2048 samples available) and based on the selection, pointers are assigned to
the corresponding buffers and the limits are configured to then go through them.

During the execution of the program, every time there is a new sample of the ADC, the * input_buffer_ptr * will be used
to save the sample in the corresponding buffer. If the FFT calculation is active, what has been saved in the output buffer will be output by the DAC, pointed to by the * buffer_output_ptr * pointer. If the FFT is not activated, the last sample taken by the ADC will be output by the DAC.

Once the input buffer is full, the FFT is computed and the upscaling operation indicated in the [CMSIS official documentation] (https://arm-software.github.io/CMSIS_5/DSP/html/group__RealFFT.html) is performed, taking into account the number of points of the FFT.

If the FFT is turned off, the pointers are reset so that the buffers have consecutive samples.
