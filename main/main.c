#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"
#include "ssd1306.h"

#define UART_TX_PIN 0
#define UART_RX_PIN 1
ssd1306_t disp;

typedef struct {
    int eixo;
    int16_t valor;
} joystick_data;

QueueHandle_t xQueueADC;

void x_task(void *p){

    static int ind = 0;
    static joystick_data dados_env;
    static int samples[5] = {0, 0, 0, 0, 0};
    static int soma = 0;
    dados_env.eixo = 0;

    while(1){
        adc_select_input(0); 
        uint16_t raw_x = adc_read();
        soma = soma - samples[ind];
        samples[ind] = raw_x; //subst amostra mais antiga pela nova leitura
        soma += samples[ind];
        int filtrado_x = soma / 5; //nova media
        int centrado_x = filtrado_x - 2047;
        int x_escala_certa = centrado_x / 8 ; 
        
        if((x_escala_certa > -30) && (x_escala_certa < 30)){ //deadzone
            dados_env.valor = 0;            
        }else{
            dados_env.valor = x_escala_certa;
        }
        
        if(dados_env.valor != 0){
            if(dados_env.valor != 0xFF && dados_env.valor != 0xff){
                //envia o struct
                xQueueSend(xQueueADC,&dados_env,0);
            }
        }

        ind = (ind + 1)% 5; //força voltar pro começo quando chegar no final do array
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void y_task(void *p){
    static int ind = 0;
    static joystick_data dados_env;
    static int samples[5] = {0, 0, 0, 0, 0};
    static int soma = 0;
    dados_env.eixo = 1;

    while(1){
        adc_select_input(1);
        uint16_t raw_y = adc_read();  
        soma = soma - samples[ind];
        samples[ind] = raw_y;
        soma += samples[ind];
        int filtrado_y = soma / 5;

        int centrado_y = filtrado_y - 2047;
        int y_escala_certa = centrado_y/8;
        
        if((y_escala_certa > -30) && (y_escala_certa < 30)){
            dados_env.valor = 0;
        }else{
            dados_env.valor = y_escala_certa;
        }
    
       if(dados_env.valor != 0){
            if(dados_env.valor != 0xFF && dados_env.valor != 0xff){
                xQueueSend(xQueueADC,&dados_env,0);
            }
        }

        ind = (ind + 1)% 5;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}



void uart_task(void *p){
    joystick_data data_receb;
    uart_init(uart0, 115200);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    while(1){
        if(xQueueReceive(xQueueADC, &data_receb, portMAX_DELAY)){
            uart_putc_raw(uart0, 0xFF); //sincroniza
            uart_putc_raw(uart0, data_receb.eixo); //eixo
            uart_putc_raw(uart0, data_receb.valor); //lsb
            uart_putc_raw(uart0, data_receb.valor >> 8);//msb
        }
    }
}

int main() {
    stdio_init_all();
    adc_init();

    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
    xQueueADC = xQueueCreate(10, sizeof(joystick_data));

    xTaskCreate(x_task, "Task 1", 1024, NULL, 1, NULL);
    xTaskCreate(y_task, "Task 2", 1024, NULL, 1, NULL);
    xTaskCreate(uart_task, "Task 3", 1024, NULL, 1, NULL);


    vTaskStartScheduler();

    while (true);
}