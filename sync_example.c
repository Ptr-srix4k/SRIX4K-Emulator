#include <avr/io.h>
#include <avr/interrupt.h>

unsigned char tx=0;

ISR(TIMER2_COMPA_vect)
{
    if (tx)
        TCCR1A = _BV(COM1A1) | _BV(WGM11);
    else
        TCCR1A = _BV(COM1A1) | _BV(COM1A0) | _BV(WGM11);

    tx = tx ^ 255;
    PORTC = tx & 1;
}

int main(void)
{
    //Set OC1A as output
    DDRB = (1 << PB1);  
    
    //Set PC0 as output
    DDRC = (1 << PC0);
    
    //Halt timers
    GTCCR = (1 << TSM)|(1 << PSRASY)|(1 << PSRSYNC);
    
    //Set timer one to Fast PWM (ICR1 = TOP)
    ICR1 = 15;
    OCR1A = 7;
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << CS10) | (1 << WGM12) | (1 << WGM13);
    
    //Set timer two to CTC mode (OCR2A = TOP)
    OCR2A = (15*8)+7;
    TIMSK2 = (1 << OCIE2A);
    TCCR2A = (1 << WGM21);
    TCCR2B = (1 << CS20);
    
    //Reset all timer
    TCNT0 = 0;
    TCNT1 = 0;
    TCNT2 = 0;
    
    //Restart all timers
    GTCCR = 0;
    
    //Enable interrupts   
    sei();
    
    while (1) 
    {
    }
}