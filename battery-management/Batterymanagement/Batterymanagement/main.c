/*
 * Projekt 4
 * Projectname: Battery Management
 * creator:     Mike M�ssner
 * date:        07.05.2017
*/
//#include <SoftwareSerial.h>  
#include <avr/interrupt.h>
#include <avr/io.h>

#define TRAN_VOLTAGE          144   //(1024/5V)*0,7V  =143,36 =~144 (cell offset)
#define CELL_VOLTAGE_MAX      880   //(1024/5V)*4,3V  =880,64 =~880
#define CELL_VOLTAGE_SOLL     850   //(1024/5V)*4,15V =849,92 =~850
#define CELL_VOLATGE_MIN      615   //(1024/5V)*3,00V =614,40 =~615
#define MIN_CURRENT           50    // ??? switches from CV => OFF
#define BALANCER_HYSTERESE    10    // Unterschied zwischen Min/Max Spannung 0,05V*(1024/5V)= 10,24
#define PWM                   OCR0A

enum Routine{Battery, Charge_CC, Charge_CV, Balance, Shutdown}state;
static char *statetext[]={"BAT", "CCC", "CCV", "BAL", "SHT"};         //DEV
volatile unsigned int second=0;     //Timeout counter
int cell[6];                        //cellVoltage
int dev_cell[6];                    //Test Cells DEV-----------------------------------------------------------
int cell_Max=0;
int cell_Min=0;
int current=0;
int inputVoltage=0;


void init_timer(){      
  //Timer 1    
  TCCR1A =  0x00;           // Clear Register (some bits were Preset...)
  TCCR1B =  (1<<WGM12);     // Clear Timer on Compare match (CTC) mode (mode 4)
  TCCR1B|=  (1<<CS12);      // start Timer1 with Prescaler = 256 (16MHz^-1 * 256 * 65536 = max ~1.04s)          // 128 with 8MHz!!!!!!!!
  OCR1A  =  62500;          // 1 sec tics (Match Register A) (16MHz/256)
  TIMSK1|=  (1<<OCIE1A);    // enable/allow Timer1 Compare A Match Interrupt Enable
  sei();                    // enable Interrupts
}

ISR(TIMER1_COMPA_vect){
    second++;
    //DEV OUTPUT                ------------------------------------------------------------------

        //DEV Laden
    for(int i=0; i<=5; i++){
      dev_cell[i]+=PWM/61;
    }                           //  ------------------------------------------------------------------


    
}

void init_PWM(void){
    //Timer 0
    TCCR0A |= (1 << COM0A1);                // Clear OC0A on Compare Match, set OC0A at BOTTOM (non-inverting mode)
    TCCR0A |= (1 << WGM01) | (1 << WGM00);  // fast PWM Mode
    TCCR0B |= (1 << CS00);                  // set prescaler to 1 and starts PWM (8MHz*(1*256bit)^-1 = 31.250 kHz
    OCR0A = 0;                              // PWM OFF
  }

void init_IO(){
  /*
 *  Port B:
 *  PB0 LED2
 *  PB1 LED1
 *  PB2 LED3
 *  PB3 MOSI
 *  PB4 MISO
 *  PB5 SCK
 *  PB6 V_Meas
 *  PB7 Selbsthaltung
 */
    DDRB  = 0b11100001;
    PORTB = (1<<7);    // Selbsthaltung ein
    
   /*
 *  Port C:
 *  PC0 ADC cell 1
 *  PC1 ADC cell 2
 *  PC2 ADC cell 3
 *  PC3 ADC cell 4
 *  PC4 ADC cell 5
 *  PC5 ADC cell 6
 *  PC6 Reset pin 
 *  PC7 ??
 */  
    DDRC  = 0x00;      // PORTC 0(PCB Pin A0) > Input
    PORTC = 0x00;      // no Pullups
    
  /*
 *  Port D:
 *  PD0 Balance cell 1
 *  PD1 Balance cell 2
 *  PD2 Balance cell 3
 *  PD3 Balance cell 4
 *  PD4 Balance cell 5
 *  PD5 Balance cell 6
 *  PD6 PWM output
 *  PD7 Engine Control output
 */  
    DDRD  = 0xFF;
    PORTD = 0x00;
  }

void init_ADC(){
    ADMUX   = (1<<REFS0);                             // URef= VCC
    ADCSRB  = 0x00;                                   // clear register
    ADCSRA  = (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);       // Prescaler 16GHz/128 = 125kHz (50-200kHz)                                f�r 8MHz  auf 64 stellen!!!!!
    ADCSRA |= (1<<ADEN);                              // Enable ADC
  }

int ADC_Read(int ch)
{
  ADMUX &= 0b11111000;      // clear MUX bit
  ADMUX |= ch;              // select new channel
  
  ADCSRA|=(1<<ADSC);        //trigger Conversion
  while(ADCSRA&(1<<ADSC))   //wait for 1st Conversion finished
  {
  }
  
  ADCSRA|=(1<<ADSC);        //trigger Conversion
  while(ADCSRA&(1<<ADSC))   //wait for 2st Conversion finished
  {
  }
  return ADC;
}


void ReadCell(void){
        // Read
        cli();//DEV (disable interrupt)------------------------------------------------------------------
        PORTD|=(1<<6);     //V_MEAS on
        cell[0]=ADC_Read(0)- TRAN_VOLTAGE;      
        cell[1]=ADC_Read(1)*2-ADC_Read(0)  - TRAN_VOLTAGE;     
        cell[2]=ADC_Read(2)*3-ADC_Read(1)*2- TRAN_VOLTAGE;
        cell[3]=ADC_Read(3)*4-ADC_Read(2)*3- TRAN_VOLTAGE;
        cell[4]=ADC_Read(4)*5-ADC_Read(3)*4- TRAN_VOLTAGE;
        cell[5]=ADC_Read(5)*6-ADC_Read(4)*5- TRAN_VOLTAGE;
        current= ADC_Read(6); 
        inputVoltage= ADC_Read(7);
        PORTD&=~(1<<6);  //V_MEAS off
        
        //DEV                     ------------------------------------------------------------------
        cell[0]=dev_cell[0]+10;
        cell[1]=dev_cell[1]+30;
        cell[2]=dev_cell[2]-20;
        cell[3]=dev_cell[3]+5;
        cell[4]=dev_cell[4]-10;
        cell[5]=dev_cell[5];
        inputVoltage=400;
        current=400;
                             //    ------------------------------------------------------------------

        
        //find Min/Max
        cell_Max=cell[0];
        cell_Min=cell[0];
        for(int i=0; i<=5; i++){ 
          if(cell[i]>cell_Max){cell_Max=cell[i];} 
          if(cell[i]<cell_Min){cell_Min=cell[i];} 
          }

        // check for Errors
        if(cell_Min<=CELL_VOLATGE_MIN){
          cell_Min=0;
          cell[0]=0;
          cell[1]=0;
          cell[2]=0;
          cell[3]=0;
          cell[4]=0;
          cell[5]=0;
        } 
        
        

        sei();//DEV (enable interrupt)
  }
  
void Statemachine(){
  static int old_Max = cell_Max;
  
  switch(state){
    
    case Battery:
      if(inputVoltage>=300){
        PORTD&=~(1<<7);         // Shutoff EngineControl (Noah)
        PORTD&=~(1<<6);         // V_MEAS off (ADC Mess Transistoren)
        PORTD&=~(0b111111);     // C1-C6 off  (Balancer Transistoren)
        PWM   = 0x00;           // PWM_OFF
        state=Charge_CC;
        }
      else{
        PORTD|=(1<<7);         // Power on EngineControl (Noah)
        }
        
      // Check if Board is in use
       if(second>=60){
          if(cell_Max+2 < old_Max){   // used more than 0.01V/min of the battery
          second=0;                   //reset Timer
          }
        }
       if(second>=600){          // Timeout after 10 min no battery usage
          state=Shutdown;  
       }
      break;  
      
    case Charge_CC:
        PORTB |= (1<<1);  //Turn on LED 1
        
        if(cell_Max>=CELL_VOLTAGE_MAX){
          state=Balance;
          }
        else if(PWM<=254){
          PWM++;  // increment current (PWM)
          }
        if(cell_Min>=CELL_VOLTAGE_SOLL){
          state=Charge_CV;
          }
        break;

    case Charge_CV:
        PORTB |= (1<<0);  //Turn on LED 2
        
        if(cell_Max>=CELL_VOLTAGE_SOLL && PWM>=1){
          PWM--; // decrement current (PWM)
        }
        else if(cell_Max<=CELL_VOLTAGE_SOLL && PWM<=254){
          PWM++;  // increment current (PWM)
          }
        if(cell_Max>=CELL_VOLTAGE_MAX){
          state=Balance;
          }
        if(current<= MIN_CURRENT){
          state=Shutdown;
          }
        break;

    case Balance:
        PWM=0x00;
        for(int i=0; i<=5; i++){
          if(cell[i]>cell_Min){
            //zelle i entladen
            PORTD |= (1<<i); //Ci on (Balancer Transistoren)
            dev_cell[i]--;                                                    //DEV
          }
          else{
            // zellentransistor schliessen 
            PORTD&=~(1<<i);     //Ci off(Balancer Transistoren)             
          }
        }
        
          if(cell_Max<=cell_Min + BALANCER_HYSTERESE){
            PORTD&=~(0b111111);     //C1-C6 off  (Balancer Transistoren)     
            state=Charge_CC;
            }
        break;
        
    case Shutdown:
          PORTB |= (1<<1);  //Turn on LED 3
          while(1){
          PORTB&=~(1<<7);   // Selbsthaltung off
          }
          break;
  }
}

void setup()
{
  init_IO();
  init_timer();
  init_ADC();
  //Serial.begin(9600);         // Begin the serial monitor (DEV)
  state=Battery;                // Start State

  for(int i=0; i<=5; i++){      //DEV
    dev_cell[i]=830;
    }
}



void loop(){
  ReadCell();
  Statemachine(); 


//DEV
/*
    Serial.println("###############################");
    Serial.print("inputVolt");
    Serial.println(analogRead(7));
    
    Serial.print("Time  : ");
    Serial.println(second);
    
    Serial.print("State : ");
    Serial.println(statetext[state]);
    
    Serial.print("MAX   : ");
    Serial.print((double)cell_Max/204,8);
    Serial.print(" V (");
    Serial.print(cell_Max);
    Serial.println(")");
    
    Serial.print("MIN   : ");
    Serial.print((double)cell_Min/204,8);
    Serial.print(" V (");
    Serial.print(cell_Min);
    Serial.println(")");
    
    Serial.print("PWM   : ");
    Serial.print((PWM*100)/256);
    Serial.println("%");
    
    Serial.print("\ncell 0: ");
    Serial.println(read_adc(0));
    Serial.print("cell 1: ");
    Serial.println(cell[1]);
    Serial.print("cell 2: ");
    Serial.println(cell[2]);
    Serial.print("cell 3: ");
    Serial.println(cell[3]);
    Serial.print("cell 4: ");
    Serial.println(cell[4]);
    Serial.print("cell 5: ");
    Serial.println(cell[5]);
    Serial.print("\n \n \n"); 
    
    delay(100);*/
 

}
