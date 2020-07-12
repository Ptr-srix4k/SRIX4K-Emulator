#define F_CPU 13560000UL 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "key.h"
#include "serial.h"
#include "crc.h"

//Timing intervals between bit transition
unsigned volatile int raw[50];
//Counter for keeping track of all the timing intervals
unsigned volatile int raw_cnt = 0;
//Detect when the receiving phase needs to stop
unsigned volatile char stop_detd = 0;
//Data buffer to transmit
unsigned volatile int data_tx[11];
//Number of data to transmit
unsigned volatile char MAX_TX=0;

unsigned volatile int tx = 1;
unsigned volatile char cnt = 2;
unsigned volatile char c=0;

unsigned volatile char max_c=0;
unsigned volatile char end_tx = 0;

//Implement rounding function with a lookup table
unsigned char find_bit(unsigned int val)
{
    if ((val>=60) && (val<192))
        return 1;
    else if ((val>=192) && (val<320))
        return 2;
    else if ((val>=320) && (val<448))
        return 3;
    else if ((val>=448) && (val<576))
        return 4;
    else if ((val>=576) && (val<704))
        return 5;
    else if ((val>=704) && (val<832))
        return 6;
    else if ((val>=832) && (val<960))
        return 7;
    else if ((val>=960) && (val<1088))
        return 8;
    else if ((val>=1088) && (val<1216))
        return 9;
    else if ((val>=1216) && (val<1344))
        return 10;
    else if ((val>=1344) && (val<1472))
        return 11;
    else if ((val>=1472) && (val<1600))
        return 12;
    else
        return 0;
}

void decode_cmd(unsigned char cmd, unsigned char add,unsigned char data_0,unsigned char data_1,unsigned char data_2,unsigned char data_3)
{
    unsigned char First,Second;
    char tx_tmp[4];
    
    switch(cmd)
    {
        case(0x06):	//Initiate
        
        data_tx[0] = 4095;
        data_tx[1] = 255;
        data_tx[2] = 768;
        data_tx[3] = (17  + 256) << 1;
        data_tx[4] = (112 + 256) << 1;
        data_tx[5] = (241 + 256) << 1;
        data_tx[6] = 0;
        data_tx[7] = 3;
        
        MAX_TX = 7;
        
        break;
        
        case(0x0E):	//Select Chip ID
        
        data_tx[0] = 4095;
        data_tx[1] = 255;
        data_tx[2] = 768;
        data_tx[3] = (17  + 256) << 1;
        data_tx[4] = (112 + 256) << 1;
        data_tx[5] = (241 + 256) << 1;
        data_tx[6] = 0;
        data_tx[7] = 3;
        
        MAX_TX = 7;
        
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
        data_tx[10] = 3;
        
        MAX_TX = 10;
        
        break;
        
        case(0x09):	//Write
            mem[add] = (unsigned long int)data_3 + ((unsigned long int)data_2 << 8) + ((unsigned long int)data_1 << 16) + ((unsigned long int)data_0 << 24);
        break;
        
        default:

        break;
    }
}

unsigned char decode_bits()	//Return 0 if no response needed
{
    unsigned int  bit_v = 0;
    unsigned int  temp = 0;
    unsigned char cbit = 0;
    unsigned char res = 0;
    unsigned char read = 0;
    unsigned char write = 0;
    unsigned char n = 0;
    unsigned int i_temp = 0;
    
    unsigned char Rx_data[5];
    unsigned char k = 0;
    
    unsigned char j=0;
    unsigned char flag_1=0;
    unsigned char flag_2=0; 
    
    //Find 10 bits and 2 bits inside the timing vector
    do 
    {
        res = find_bit(raw[j]);
        if (res == 10)
            flag_1 = 1;
        else if (flag_1 & (res == 2))
            flag_2 = 1;
        else
            flag_1 = 0;
        j++;
    } while (!flag_2 && (j<raw_cnt));
    
    i_temp = raw_cnt;
    
    //Decode timing vector
    for (n=j;n<i_temp;n++)
    {
        res = find_bit(raw[n]);      
         
        temp = (temp >> res) + (bit_v*((1<<res) - 1) << (10-res));
        cbit = cbit + res;
        if (cbit==11)
        {
            temp = temp & 255;
            if (read)
            {
                decode_cmd(0x08,(unsigned char)temp,0x00,0x00,0x00,0x00);
                break;
            }
            else if (write)
            {
                Rx_data[k] = temp;
                k++;
                if (k>4)
                {
                    decode_cmd(0x09,Rx_data[0],Rx_data[1],Rx_data[2],Rx_data[3],Rx_data[4]);
                    return 0;
                }
            }
            else
            {
                if (temp==0x04 || temp==0x0F || temp==0x0C)
                    return 0;
                else if (temp==0x08)
                    read = 1;
                else if (temp==0x09)
                    write = 1;
                else
                    decode_cmd((unsigned char)temp,0x00,0x00,0x00,0x00,0x00);            
            }
            cbit = 0;
            temp = 0;
        }
        bit_v = bit_v ^ 1;
    }
    
    return 1;
}

//Comparator setup
void setup_comparator()
{
    //Select AN1 as comparator input (-)
    ADCSRB = 0;    
    //Analog Comparator Interrupt Enable 
    ACSR   = (1 << ACIE); 
    //AIN1, AIN0 Digital Input Disable
    DIDR1  = (1 << AIN1D) | (1 << AIN0D);   
}

//Setup input/output pins
void setup_pins()
{
    //Set all PORTD as input (AN1/AN0) except TX1 and PD5
    DDRD = (1 << PD1) | (1 << PD5); 
    //Disable input pull-up resistor       
    PORTD = 0;
    //Set PB0 as output (for debug)
    DDRB = (1 << PB0) | (1 << PB1);    
    //Set PortB to zero
    PORTB = 0;
}

//Setup timer1
void setup_timer1()
{    
    TCNT1 = 0;
    TIMSK1 = 0;
    TCCR1A = 0;
    //No prescaler
    TCCR1B = (1 << CS10);
}

//Setup timer0
void setup_timer0()
{
    //Set OC0B on compare match, clear OC0B at BOTTOM
    TCCR0A = (1 << COM0B1) | (1 << COM0B0) | (1 << WGM01) | (1 << WGM00);
    //No prescaler
    TCCR0B = (1 << CS00);
    OCR0B  = 160; //Seems OK
    TIMSK0 = 0;
    TCNT0 = 0;
}

void print_raw_debug()
{
    unsigned char k;
    for(k=0;k<raw_cnt;k++)
    {
        Serial_TX(raw[k]);
    }
}

void print_data_debug()
{
    unsigned char k;
    for(k=0;k<MAX_TX;k++)
    {
        Serial_TX(data_tx[k]);
    }
}

int main(void)
{
    //unsigned char i=0;
    
    USART_Init(352);//2406 baud
    
    //Comparator setup
    setup_comparator();
    //Setup input/output pins
    setup_pins();
    //Setup timer1
    setup_timer1();
    //Setup timer0 (PWM signal)
    setup_timer0();
    //Set interrupt flag
    sei();
    
    while (1) 
    {
       if (stop_detd >= 2)
       {
           //Stop Timer1 counter
           TCCR1A = 0;
           TCCR1B = 0;
           //Clear Timer1 overflow interrupt
           TIFR1 = (1 << TOV1);
           //Reset Timer1
           TCNT1 = 0;
           
           //Clear comparator interrupt flag 
           ACSR = (1 << ACI);
           
           //Clear stop counter flag
           stop_detd = 0;  
           
           if (decode_bits())
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
           
           raw_cnt = 0;
           
           //Enable Timer1
           TCCR1B = (1 << CS10);
           //Analog Comparator Interrupt Enable
           ACSR   = (1 << ACIE);
       } 
    }
}

//ADC comparator interrupt
ISR(ANALOG_COMP_vect)
{
    //Stop the counter
    TCCR1B = 0;
    //Save data only if timer1 did not overflow
	if (!((TIFR1 >> TOV1) & 1))
		raw[raw_cnt++] = TCNT1;
    
    //Reset stop counter when overflow
    if ((TIFR1 >> TOV1) & 1)
        stop_detd = 0;
    else if (TCNT1>1260)
        stop_detd++;
          
    //Clear overflow flag    
	TIFR1 = (1 << TOV1);
    //Reset the counter
	TCNT1 = 0;
    //Start the counter
	TCCR1B = (1 << CS10);
}

ISR(TIMER2_COMPA_vect)
{
    if (tx)
        TCCR1A = _BV(COM1A1) | _BV(WGM11);
    else
        TCCR1A = _BV(COM1A1) | _BV(COM1A0) | _BV(WGM11);

    tx = data_tx[c] & 1;
    data_tx[c] = data_tx[c] >> 1;
    
    if (c==MAX_TX && cnt==1)
    {
        cnt = 2;
        c = 0;
        end_tx = 1;
        TCCR1B = 0;
    }
    else if (cnt == 9)
    {
        c++;
        cnt = 0;
    }
    else
        cnt++;
}




