#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"
#include "ssd1306.h"

typedef struct {
    uint8_t eixo;
    uint16_t valor;
} joystick_data;

QueueHandle_t xQueueADC;


void x_task(void *p){
    int samples[5] = {0};
    long soma = 0;
    int ind = 0;
    joystick_data dados_env;
    dados_env.eixo = 0;

    while (1){
        adc_select_input(0);
        int raw_x = adc_read();


        soma -= samples[ind];
        samples[ind] = raw_x; //subst amostra mais antiga pela nova leitura
        soma += samples[ind];
        ind = (ind + 1)% 5; //força voltar pro começo quando chegar no final do array
        
        int filtrado_x = soma / 5; //nova media
        int centrado_x = filtrado_x - 2047;
        int x_escala_certa = (centrado_x * 255)/2047; //255 -> ajusta a escala; 2047 -> ponto central (metade do valor máximo, já que seria de -2047 a +2047)

        if(x_escala_certa > -100 && x_escala_certa < 100){ //100 -> deadzone 
            x_escala_certa = 0;
        }

        //envia o struct
        dados_env.valor = x_escala_certa;
        xQueueSend(xQueueADC, &dados_env, 0);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void y_task(void *p){
    int samples[5] = {0};
    long soma = 0;
    int ind = 0;
    joystick_data dados_env;
    dados_env.eixo = 1;

    while (1){
        adc_select_input(1);
        int raw_y = adc_read();

        soma -= samples[ind];
        samples[ind] = raw_y;
        soma += samples[ind];
        ind = (ind + 1)% 5;
        
        int filtrado_y = soma / 5;
        int centrado_y = -(filtrado_y - 2047);  //inverte o eixo y
        int y_escala_certa = (centrado_y * 255)/2047;
        
        if(y_escala_certa > -100 && y_escala_certa < 100){
            y_escala_certa = 0;
        }
        
        dados_env.valor = y_escala_certa;
        xQueueSend(xQueueADC, &dados_env, 0);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void uart_task(void *p){
    joystick_data data_receb;
    const uint8_t EOP = 0xFF;

    while (1){
        if(xQueueReceive(xQueueADC, &data_receb, portMAX_DELAY) == pdTRUE){
            //atualiza o valor do eixo dependendo do respectivo eixo
            uint8_t val_lsb = data_receb.valor & 0xFF; //compara bit a bit com 255
            uint8_t val_msb = (data_receb.valor >> 8) & 0xFF; //faz o bitshift pra colocar no lugar certo

            printf("%c%c%c%c", data_receb.eixo, val_msb, val_lsb, EOP);
        } 
    }
}

int main(){
    stdio_init_all();

    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);

    xQueueADC = xQueueCreate(10, sizeof(joystick_data));

    xTaskCreate(x_task, "X Task", 256, NULL, 1, NULL);
    xTaskCreate(y_task, "Y Task", 256, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}