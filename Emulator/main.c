#define F_CPU 13560000UL 
#define START_CNT 4
#define TOGGLE_PC0 PINC = (1 << PC0)

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "key.h"
#include "serial.h"
#include "crc.h"

//Data buffer to transmit
unsigned volatile int data_tx[15];
//Number of data to transmit
unsigned volatile char MAX_TX=0;

unsigned volatile char tx = 1;
unsigned volatile char cnt = START_CNT;
unsigned volatile char c=0;
unsigned volatile char end_tx = 0;

unsigned char rx_buffer[50];
unsigned char num_rx=0;

unsigned char chip_id = 7;
unsigned char slot_num = 7;

void USART_Init(unsigned int ubrr)
{
    /*Set baud rate */
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    /*Enable receiver */
    UCSR0B = (1<<RXEN0);
    /* Set frame format: 8data, 1stop bit */
    UCSR0C = (3<<UCSZ00);
}

void USART_Flush(void)
{
    unsigned char dummy;
    while (UCSR0A & (1<<RXC0)) dummy = UDR0;
}

unsigned char decode_cmd()
{
    unsigned char First,Second;
    unsigned char cmd, add, data_0, data_1, data_2, data_3;
    unsigned char tx_tmp[8];
    
    cmd    = rx_buffer[0];
    add    = rx_buffer[1];
    data_0 = rx_buffer[2];
    data_1 = rx_buffer[3];
    data_2 = rx_buffer[4];
    data_3 = rx_buffer[5];
    
    switch(cmd)
    {      
        case(0x06):	
        
            if(add == 0x00) //Initiate
            {
                //Create pseudorandom chip_id
                chip_id = (chip_id << 1) | ((((chip_id >> 1) ^ (chip_id >> 2)) ^ ((chip_id >> 3) ^ (chip_id >> 7))) & 0x01);
                slot_num = (chip_id & 0x0F); 
        
                data_tx[0] = 4095;
                data_tx[1] = 255;
                data_tx[2] = 768;
                data_tx[3] = (chip_id | 256) << 1;  
                
                tx_tmp[0] = chip_id;   
                
                ComputeCrc(tx_tmp, 1, &First, &Second);
                
                data_tx[4] = (First  | 256) << 1;
                data_tx[5] = (Second | 256) << 1;
                data_tx[6] = 0;
                data_tx[7] = 7;
            
                MAX_TX = 7;
            
                return 1;
            }
            else if(add == 0x04) //PCALL16
            {
                if (slot_num == 0)
                {                    
                    data_tx[0] = 4095;
                    data_tx[1] = 255;
                    data_tx[2] = 768;
                    data_tx[3] = (chip_id | 256) << 1;  
                
                    tx_tmp[0] = chip_id;
                    
                    ComputeCrc(tx_tmp, 1, &First, &Second);
                
                    data_tx[4] = (First  | 256) << 1;
                    data_tx[5] = (Second | 256) << 1;
                    data_tx[6] = 0;
                    data_tx[7] = 7;
                
                    MAX_TX = 7;
                
                    return 1;
                }
                else
                {
                    slot_num = ((slot_num << 1) | ((((slot_num) ^ (slot_num >> 3))) & 0x01)) & 0x0F;
                    return 0;
                }
            }
            else
                return 0;
        
        break;
        
        //Slot marker 
        case(0x16):
        case(0x26): 
        case(0x36):
        case(0x46):
        case(0x56):
        case(0x66):
        case(0x76):
        case(0x86):
        case(0x96):
        case(0xa6):
        case(0xb6): 
        case(0xc6):
        case(0xd6):
        case(0xe6):
        case(0xf6):
        
            if (((cmd >> 4) & 0x0F) == (slot_num & 0x0F))
            {
                data_tx[0] = 4095;
                data_tx[1] = 255;
                data_tx[2] = 768;
                data_tx[3] = (chip_id | 256) << 1;  
                
                tx_tmp[0] = chip_id;  
                
                ComputeCrc(tx_tmp, 1, &First, &Second);
                
                data_tx[4] = (First  | 256) << 1;
                data_tx[5] = (Second | 256) << 1;
                data_tx[6] = 0;
                data_tx[7] = 7;
                
                MAX_TX = 7;
                
                return 1;
            }
            else
                return 0;
        
        break;

        case(0x0E):	//Select Chip ID
        
            if (add == chip_id)
            {
                data_tx[0] = 4095;
                data_tx[1] = 255;
                data_tx[2] = 768;
                data_tx[3] = (chip_id | 256) << 1;
                
                tx_tmp[0] = chip_id;
                
                ComputeCrc(tx_tmp, 1, &First, &Second);
                
                data_tx[4] = (First  | 256) << 1;
                data_tx[5] = (Second | 256) << 1;
                data_tx[6] = 0;
                data_tx[7] = 7;
                
                MAX_TX = 7;
                
                return 1;
            }
            else
                return 0;
        
        break;
        
        case(0x08):	//Read
        
            data_tx[0] = 4095;
            data_tx[1] = 255;
            data_tx[2] = 768;
            
            if (add==0xFF)
            {
                tx_tmp[3] = (mem_FF       & 255);
                tx_tmp[2] = ((mem_FF>>8)  & 255);
                tx_tmp[1] = ((mem_FF>>16) & 255);
                tx_tmp[0] = ((mem_FF>>24) & 255);
            }
            else
            {
                tx_tmp[3] = (mem[add]       & 255);
                tx_tmp[2] = ((mem[add]>>8)  & 255);
                tx_tmp[1] = ((mem[add]>>16) & 255);
                tx_tmp[0] = ((mem[add]>>24) & 255);
            }
            
            ComputeCrc(tx_tmp, 4, &First, &Second);
            
            data_tx[3] = (tx_tmp[0] | 256) << 1;
            data_tx[4] = (tx_tmp[1] | 256) << 1;
            data_tx[5] = (tx_tmp[2] | 256) << 1;
            data_tx[6] = (tx_tmp[3] | 256) << 1;
            data_tx[7] = (First     | 256) << 1;
            data_tx[8] = (Second    | 256) << 1;

            data_tx[9] = 0;
            data_tx[10] = 7;
            
            MAX_TX = 10;
            
            return 1;
        
        break;
        
        case(0x09):	//Write
            mem[add] = (unsigned long int)data_3 + ((unsigned long int)data_2 << 8) + ((unsigned long int)data_1 << 16) + ((unsigned long int)data_0 << 24);
            
            return 0;
        break;
        
        case(0x0B):	//GET_UID
        
            data_tx[0] = 4095;
            data_tx[1] = 255;
            data_tx[2] = 768;
            
            tx_tmp[7] = ((UID[0]>>24) & 255);
            tx_tmp[6] = ((UID[0]>>16) & 255);
            tx_tmp[5] = ((UID[0]>>8)  & 255);
            tx_tmp[4] = (UID[0]       & 255);
            tx_tmp[3] = ((UID[1]>>24) & 255);
            tx_tmp[2] = ((UID[1]>>16) & 255);
            tx_tmp[1] = ((UID[1]>>8)  & 255);
            tx_tmp[0] = (UID[1]       & 255);
            
            ComputeCrc(tx_tmp, 8, &First, &Second);
            
            data_tx[3]  = (tx_tmp[0] | 256) << 1;
            data_tx[4]  = (tx_tmp[1] | 256) << 1;
            data_tx[5]  = (tx_tmp[2] | 256) << 1;
            data_tx[6]  = (tx_tmp[3] | 256) << 1;
            data_tx[7]  = (tx_tmp[4] | 256) << 1;
            data_tx[8]  = (tx_tmp[5] | 256) << 1;
            data_tx[9]  = (tx_tmp[6] | 256) << 1;
            data_tx[10] = (tx_tmp[7] | 256) << 1;
            data_tx[11] = (First     | 256) << 1;
            data_tx[12] = (Second    | 256) << 1;
            
            data_tx[13] = 0;
            data_tx[14] = 7;
            
            MAX_TX = 14;
            
            return 1;
            
        break;
        
        default:
            return 0;
        break;
    }
}

//Setup input/output pins
void setup_pins()
{
    //TXD UART as output
    DDRD = (1 << PD1); 
    //Disable input pull-up resistor       
    PORTD = 0;
    //Set PB0 as software UART output / PB1 as modulator output
    DDRB = (1 << PB0) | (1 << PB1);    
    //Set PortB to zero
    PORTB = 0;
    
    DDRC = (1 << PC0);
    PORTC = 0;
}

//Setup timer1
void setup_timer1()
{    
    //Stop Timer1 counter
    TCCR1A = 0;
    TCCR1B = 0;
    //Clear Timer1 overflow interrupt
    TIFR1 = (1 << TOV1);
    //Reset Timer1
    TCNT1 = 0;
    
    //Disable Timer0
    TCCR0B = 0;
    TCNT0 = 0;
}

//Setup timer0
void setup_timer0()
{    
    //Start Timer0 counter
    TCCR0A = 0;
    TCCR0B = (1 << CS01) | (1 << CS00);
    //Clear Timer0 overflow interrupt
    TIFR0 = (1 << TOV0);
    //Set Timer0
    TCNT0 = 256-30;
}

void print_debug_rx()
{
    unsigned char i=0;
    char str[16];
    
    for(i=0;i<num_rx;i++)
    {
        utoa(rx_buffer[i],str, 10);   
        UART_tx_str(str);
        while(tx_shift_reg);
        UART_tx_str("\n\r");
    }  

    UART_tx_str("\n\r");    
}

int main(void)
{    
    unsigned char status;
    unsigned char data;
    unsigned char SOF=0;
    unsigned char EnOF=0;
    unsigned char invalid=0;
    unsigned char state=0;
    unsigned char error=0;

    USART_Init(7);//106000 baud
    
    //Setup input/output pins
    setup_pins();
    //Setup timer1
    setup_timer1();
    //Setup hardware UART
    USART_Init(7);
    //Set interrupt flag
    sei();
    
    while (1) 
    {
        //=================================
        // RECEIVE DATA
        //=================================    
        setup_timer0();
        //Wait data from serial port
        while (!(UCSR0A & (1<<RXC0))) {
            if (TIFR0 & 0x01) //Check timer overflow
            {
                //Reset logic
                state = 0;    
                num_rx = 0;
                error = 1;
                break;
            }        
        } 
        status = UCSR0A;
        data = UDR0;
        if (status & ((1<<FE0))) //Stop bit = 0
            invalid = 1;
        
        //Stop timer0
        TCCR0B = 0;
        
        switch (state)
        {
            case(0):  //Wait SOF    
                if (invalid && !error)
                    state = 1;
                else
                    state = 0;
            break;      
            case(1):  //RX DATA   
                if (invalid && !error)
                {
                    EnOF = 1;
                    state = 0;
                }
                else
                {
                    //Save data
                    rx_buffer[num_rx] = data;
                    num_rx++;    
                }
            break;      
        }
        
        invalid = 0;
        error = 0;
        //=================================
        //================================= 
        if (EnOF)
        {      
            state = 0;
            UCSR0B = 0; //Disable RX UART
            
            //Wait some time (from CRX14 datasheet => 75us MIN)
            _delay_us(90); 
            
            if (decode_cmd())
            {
                tx = 1;
                
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
                
                //Wait till transmission ends
                while(!end_tx){}                
                end_tx = 0;
                
                TIFR2 = (1 << OCF2A);
                TCCR2A = 0;
                TCCR2B = 0;
                TCCR1A = 0;
                TCCR1B = 0;
                TIMSK1 = 0;
                TIMSK2 = 0;    
            }
            
            _delay_us(5);
          
            num_rx = 0;  
            SOF = 0;
            EnOF = 0;
            //Enable RX UART
            UCSR0B = (1<<RXEN0);
            
        } 
    }
}

ISR(TIMER2_COMPA_vect)
{
    if (tx)
        TCCR1A = _BV(COM1A1) | _BV(WGM11);
    else
        TCCR1A = _BV(COM1A1) | _BV(COM1A0) | _BV(WGM11);

    tx = data_tx[c] & 1;
    data_tx[c] = data_tx[c] >> 1;
    
    if (c==MAX_TX && cnt==3)
    {
        TCCR1B = 0;
        TCCR1A = _BV(COM1A1) | _BV(COM1A0) | _BV(WGM11);
        PORTB = 0;
        cnt = START_CNT;
        c = 0;
        end_tx = 1;  
    }
    else if (cnt == 9)
    {
        c++;
        cnt = 0;
    }
    else
        cnt++;
}




