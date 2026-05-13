#include "ch32v30x_misc.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void NMI_Handler(void) {
    NVIC_SystemReset();
    for (;;) {

    }
}

void HardFault_Handler(void) {
    NVIC_SystemReset();
    for (;;) {
      
    }
}

__attribute__((interrupt("WCH-Interrupt-fast"), used))
void TIM2_IRQHandler(void);

__attribute__((interrupt("WCH-Interrupt-fast"), used))
void USBHS_IRQHandler(void);

__attribute__((interrupt("WCH-Interrupt-fast"), used))
void I2C2_EV_IRQHandler(void);

__attribute__((interrupt("WCH-Interrupt-fast"), used))
void I2C2_ER_IRQHandler(void);
