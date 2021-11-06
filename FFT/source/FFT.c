/*
 * Autor: Santiago Raimondi
 * Fecha: 5/11/2021
 *
 * Descripcion: Trabajo Práctico 3. Consigna:
 *
 *	Realizar un programa aplicativo que sea capaz de aplicar la FFT a un buffer de memoria de diferente tamaño
 *	(Ej: 512 muestras, 1024 y 2048), que es adquirido con el Laboratorio 1 teniendo en cuenta además los cambios
 *	de velocidades de muestreo.
 *
 *	Con una de las teclas de la placa de evaluación, se habilitará la aplicación de la FFT o se hace bypass de la
 *	misma a un buffer de salida distinto del buffer de entrada.
 *
 *	Transmitir el buffer (Resultado de la FFT) por el puerto serie (SCI) y graficar en la PC con alguna aplicación
 *	(Ej. Una GUI de MATLAB), el espectro obtenido.
 *
 *	Saque conclusiones respecto de la implementación de la FFT cuando cambia la frecuencia de muestreo o tamaño de
 *	ventana de muestras.
 *
 *
 * NOTAS:
 *
 *
 */

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "peripherals.h"
#include "fsl_debug_console.h"
#include "arm_math.h"
#include <stdio.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define SAMPLE_FRECUENCY_TICKS_8KS  (uint32_t) 7499	/* Cantidad de ticks para que con un reloj de periferico de 60 [MHz] se obtenga la frecuencia de interrupcion deseada */
#define SAMPLE_FRECUENCY_TICKS_16KS (uint32_t) 3749
#define SAMPLE_FRECUENCY_TICKS_22KS (uint32_t) 2726
#define SAMPLE_FRECUENCY_TICKS_44KS (uint32_t) 1363
#define SAMPLE_FRECUENCY_TICKS_48KS (uint32_t) 1249

#define FFT_SAMPLES_512  (uint32_t) 512
#define FFT_SAMPLES_1024 (uint32_t) 1024
#define FFT_SAMPLES_2048 (uint32_t) 2048
#define I_FFT_FLAG_R 	 (uint32_t) 0		/* Flag que indica que se hara la Forward FFT*/
#define BIT_REVERSE_FLAG (uint32_t) 1		/* Flag que indica que no se invertira el orden de salida de los bits de la FFT */

#define CHANNEL_GROUP 0U
#define DC_OFFSET (uint16_t) 32767	/* Offset de DC que trae la señal. */
#define DAC_BUFFER_INDEX 0U			/* Como no se utiliza el buffer, siempre es 0 este valor */

#define __DEBUGG (bool) false	/* Variable de deuggeo */

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* Variables para el hardware */
q15_t input_value_fixed = 0;	/* Valor en formato q15 leido por el ADC */
bool adc_finished = false;		/* Bandera para avisar que hay una nueva conversion de ADC */
volatile uint8_t counter = 0;			/* Contador de interrupciones de cambio de velocidad */
volatile bool fft_is_active = true;		/* Bandera que señala si se esta aplicando la FFT a la señal de entrada */

/* Buffers de entrada y salida para cada FFT */
q15_t input_buffer_512	[FFT_SAMPLES_512];
q15_t output_buffer_512	[FFT_SAMPLES_512];
q15_t input_buffer_1024	[FFT_SAMPLES_1024];
q15_t output_buffer_1024[FFT_SAMPLES_1024];
q15_t input_buffer_2048	[FFT_SAMPLES_2048];
q15_t output_buffer_2048[FFT_SAMPLES_2048];

/* Punteros que se usan para recorrer los buffers */
volatile q15_t *input_buffer_ptr, *output_buffer_ptr;
q15_t *input_start_ptr, *output_start_ptr;	/* Punteros auxiliares que siempre señalan el incio del buffer */

/* Variables para la logica de los buffers */
volatile uint16_t samples_counter = 0;	/* Variable  que se usa para contar la cantidad de muestras tomadas */
uint16_t buffer_limit 	= 0;			/* Limite que se asignara al momento de la seleccion de la cantidad de puntos de la FFT */
bool buffer_is_full = false;	/* Bandera que indica si se lleno el buffer de muestras de entradas para computar la FFT */

/* Instancias de la estructura de configuracion para las distintas FFT */
arm_rfft_instance_q15 fft_512;
arm_rfft_instance_q15 fft_1024;
arm_rfft_instance_q15 fft_2048;
uint8_t upscale_bits = 0;	/* Bits necesarios para volver la salida de la FFT a formato q15 segun la documentacion de */

/*******************************************************************************
 * Code
 ******************************************************************************/

/*
 * ISR_SW2: Se activa o desactiva la FFT (se hace bypass a un buffer de salida distinto).
 * NO se desactiva el muestreador.
*/
void GPIOC_IRQHANDLER(void)
{
	/* Get pin flags */
	uint32_t pin_flags = GPIO_PortGetInterruptFlags(GPIOC);

	/* Place your interrupt code here */

	if (fft_is_active)
	{
		/* 	Se desactiva el calculo de la FFT y se dejan listos los punteros en la posicion 
			inicial del buff. correspondiente, para que cuando se active nuevamente los buffers 
			se llenen con muestras consecutivas.
		*/

		fft_is_active = false;

		input_buffer_ptr  = input_start_ptr;
		output_buffer_ptr = output_start_ptr;
		buffer_is_full = false;
	}
	else
	{
		fft_is_active = true;
		samples_counter = 0;
		buffer_is_full = false;
	}
		
	/* Clear pin flags */
	GPIO_PortClearInterruptFlags(GPIOC, pin_flags);

	/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
	 Store immediate overlapping exception return operation might vector to incorrect interrupt. */
    SDK_ISR_EXIT_BARRIER;
}

/*
 * ISR_SW3: Se modifica la velocidad de muestreo y enciende LEDs indicadores.
*/
void GPIOA_IRQHANDLER(void)
{
	/* Get pin flags */
	uint32_t pin_flags = GPIO_PortGetInterruptFlags(GPIOA);

	/* Place your interrupt code here */

	/* Apago los LEDs para que se enciendan solo los necesarios */
	LED_RED_OFF();
	LED_GREEN_OFF();
	LED_BLUE_OFF();

	counter++; /* Se incrementa el contador de velocidad de muestreo */

	if(counter > 4)	/* Solo hay 5 velocidades, se reinicia en forma circular */
	{
	  counter = 0;
	}

	/* De acuerdo al estado del contador, se afecta la frecuencia de interr. del PIT */
	if(counter == 0)
	{
		PIT_SetTimerPeriod(PIT_PERIPHERAL, PIT_CHANNEL_0, SAMPLE_FRECUENCY_TICKS_8KS); /* 8 [k/S] */

		LED_RED_TOGGLE();
	}

	if(counter == 1)
	{
		PIT_SetTimerPeriod(PIT_PERIPHERAL, PIT_CHANNEL_0, SAMPLE_FRECUENCY_TICKS_16KS); /* 16 [k/S] */

		LED_BLUE_TOGGLE();
	}

	if(counter == 2)
	{
		PIT_SetTimerPeriod(PIT_PERIPHERAL, PIT_CHANNEL_0, SAMPLE_FRECUENCY_TICKS_22KS); /* 22 [k/S] */

		LED_GREEN_TOGGLE();
	}

	if(counter == 3)
	{
		PIT_SetTimerPeriod(PIT_PERIPHERAL, PIT_CHANNEL_0, SAMPLE_FRECUENCY_TICKS_44KS); /* 44 [k/S] */

		LED_BLUE_TOGGLE();
		LED_RED_TOGGLE();
	}

	if(counter == 4)
	{
		PIT_SetTimerPeriod(PIT_PERIPHERAL, PIT_CHANNEL_0, SAMPLE_FRECUENCY_TICKS_48KS); /* 48 [k/S] */

		LED_RED_TOGGLE();
		LED_GREEN_TOGGLE();
	}

	/* Clear pin flags */
	GPIO_PortClearInterruptFlags(GPIOA, pin_flags);

	// Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
	// Store immediate overlapping exception return operation might vector to incorrect interrupt.
    SDK_ISR_EXIT_BARRIER;
}

/*
 * ISR_PIT0: Se dispara la conversion del ADC0.
*/
void PIT_CHANNEL_0_IRQHANDLER(void)
{
	uint32_t intStatus;
	/* Reading all interrupt flags of status register */
	intStatus = PIT_GetStatusFlags(PIT_PERIPHERAL, PIT_CHANNEL_0);
	PIT_ClearStatusFlags(PIT_PERIPHERAL, PIT_CHANNEL_0, intStatus);

	/* Place your code here */

	/* Se dispara la conversion */
	ADC16_SetChannelConfig(ADC0_PERIPHERAL, CHANNEL_GROUP, &ADC0_channelsConfig[0]);	/* De la descripcion de la funcion vi que el CHANNEL_GROUP=0 para software trigger */

	// Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
	// Store immediate overlapping exception return operation might vector to incorrect interrupt.
    SDK_ISR_EXIT_BARRIER;
}

/*
 * ISR_ADC: Se guarda el valor muestreado y se lo muestra por DAC0.
*/
void ADC0_IRQHANDLER(void)
{
	/* Array of result values*/
	uint32_t result_value = 0;

	uint32_t status = ADC16_GetChannelStatusFlags(ADC0_PERIPHERAL, CHANNEL_GROUP);
	if ( status == kADC16_ChannelConversionDoneFlag){
		result_value = ADC16_GetChannelConversionValue(ADC0_PERIPHERAL, CHANNEL_GROUP);
	}

	/* Place your code here */
	adc_finished = true;	
	input_value_fixed =  (q15_t)(result_value - DC_OFFSET);	/* Se quita el offset de continua sobre el que viene montada la señal */

	/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
	 Store immediate overlapping exception return operation might vector to incorrect interrupt. */
    SDK_ISR_EXIT_BARRIER;
}

/*!
 * @brief Main function
 */
int main(void)
{
    /* Board pin init */
	BOARD_InitBootPins();
    BOARD_InitBootClocks();
	BOARD_InitBootPeripherals();
	BOARD_InitDebugConsole();	/* Para hacer los PRINTF */

	PRINTF("\r\nMuestra de TP3 DSP.\r\n");

    /* El LED RGB es activo por bajo, por lo que al setear el puerto estoy apagandolos */
	LED_BLUE_OFF();
	LED_GREEN_OFF();
	/* Arranco en la menor velocidad de muestreo */
	LED_RED_ON();

	volatile uint8_t seleccion = 0;

    PRINTF("Seleccione la cantidad de muestras de la FFT.\r\n");
    PRINTF("1: FFT de 512\r\n");
    PRINTF("2: FFT de 1024\r\n");
    PRINTF("3: FFT de 2048\r\n");
    SCANF("%d",&seleccion);

    /* Se establece la configuracion de la FFT correspondiente y componentes asociados */
	if (seleccion == 1)
	{
		input_buffer_ptr = &input_buffer_512[0];
		output_buffer_ptr = &output_buffer_512[0];
		input_start_ptr = &input_buffer_512[0];
		output_start_ptr = &output_buffer_512[0];
		buffer_limit = FFT_SAMPLES_512;
		upscale_bits = 8;
		if (arm_rfft_init_q15(&fft_512, FFT_SAMPLES_512, I_FFT_FLAG_R, BIT_REVERSE_FLAG) != ARM_MATH_SUCCESS)
		{
			PRINTF("ERROR EN INICIALIZACION DE FFT. \r\n");
			while (1){}
		}
	}
	if (seleccion == 2)
	{
		input_buffer_ptr = &input_buffer_1024[0];
		output_buffer_ptr = &output_buffer_1024[0];
		input_start_ptr = &input_buffer_1024[0];
		output_start_ptr = &output_buffer_1024[0];
		buffer_limit = FFT_SAMPLES_1024;
		upscale_bits = 9;
		if (arm_rfft_init_q15(&fft_1024, FFT_SAMPLES_1024, I_FFT_FLAG_R, BIT_REVERSE_FLAG) != ARM_MATH_SUCCESS)
		{
			PRINTF("ERROR EN INICIALIZACION DE FFT. \r\n");
			while (1){}
		}
	}
	if (seleccion == 3)
	{
		input_buffer_ptr = &input_buffer_2048[0];
		output_buffer_ptr = &output_buffer_2048[0];
		input_start_ptr = &input_buffer_2048[0];
		output_start_ptr = &output_buffer_2048[0];
		buffer_limit = FFT_SAMPLES_2048;
		upscale_bits = 10;
		if (arm_rfft_init_q15(&fft_2048, FFT_SAMPLES_2048, I_FFT_FLAG_R, BIT_REVERSE_FLAG)!= ARM_MATH_SUCCESS)
		{
			PRINTF("ERROR EN INICIALIZACION DE FFT. \r\n");
			while (1){}
		}
	}
	/* Se verifica que la seleccion haya sido correcta */
	if (buffer_limit == 0)
	{
		PRINTF(" ERROR DE SELECCION. \r\n");
		PRINTF(" REINICIE LA PLACA E INTENTE NUEVAMENTE. \r\n");
		while (1){}	
	}

	while (1)
    {
		if (adc_finished && !buffer_is_full)
		{
			adc_finished = false;

			*input_buffer_ptr = input_value_fixed;	/* Se guarda el nuevo valor y se desplaza el puntero */
			input_buffer_ptr ++;

			if (fft_is_active)
			{
				/* Se actualiza el valor del DAC con lo que haya en el buffer de salida de la FFT */
				DAC_SetBufferValue(DAC0, DAC_BUFFER_INDEX, ((*output_buffer_ptr + DC_OFFSET)>>4));	
				output_buffer_ptr ++;
			}
			else
				DAC_SetBufferValue(DAC0, DAC_BUFFER_INDEX, ((input_value_fixed + DC_OFFSET)>>4));
			
			samples_counter ++;
			if(samples_counter >= buffer_limit)
			{
				samples_counter = 0;
				buffer_is_full = true;	/* Se avisa que se lleno el buffer */
			}
		}

		if(buffer_is_full)
		{
			buffer_is_full = false;

			/* De acuerdo a la seleccion, se calcula la FFT si está activada y se hace la operacion de upscale.
			 * Al final se vuelven a establecer los punteros.
			 */

			uint32_t primask_value = DisableGlobalIRQ(); /* Desactivo interr. hasta que termine de computar */

			if(seleccion == 1)
			{
				if (fft_is_active)
				{
					arm_rfft_q15(&fft_512, input_buffer_512, output_buffer_512);

					for(uint16_t i = 0; i < buffer_limit; i++)
						output_buffer_512[i] <<= upscale_bits;
				}
			}
			if(seleccion == 2)
			{
				if (fft_is_active)
				{
					arm_rfft_q15(&fft_1024, input_buffer_1024, output_buffer_1024);

					for(uint16_t i = 0; i < buffer_limit; i++)
						output_buffer_512[i] <<= upscale_bits;
				}
			}
			if(seleccion == 3)
			{
				if (fft_is_active)
				{
					arm_rfft_q15(&fft_2048, input_buffer_2048, output_buffer_2048);

					for(uint16_t i = 0; i < buffer_limit; i++)
						output_buffer_512[i] <<= upscale_bits;
				}
			}
	
			input_buffer_ptr  = input_start_ptr;
			output_buffer_ptr = output_start_ptr;

			EnableGlobalIRQ(primask_value);	/* Activo las interr. nuevamente */
		}

		/* TODO: Hacer la transmision de datos */
    }
}


