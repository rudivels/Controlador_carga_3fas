/*
 *   Versao Dummy Load Control V1.1 @ Rudi 15/11/2019
 *   Placa definitivo para Pico Central Hidrelétrico 
 *   Timer 1 contador geral 
 *   Sincronismo pelo Interupcao no INT0 - Pino 2 Arduino 
 *   Saidas nos pinos 
 *   Fase R = pino 5 Arduino 
 *   Fase S = pino 6 Arduino
 *   Fase T = pino 7 Arduino
 *   
 *   Essa versao precisa de um pulso de 20 microsegundos para ativar o TRIAC
 *   Vamos ter que ver uma maneira mais elegante para eliminar o delay de microsegundos na interupcao
 *   Também só trabalha com meia onda
 *   
 *   Cada meio ciclo AC corresponde a cerca de 520 contagens do Timer1 do Arduino 
 *   pois o clock de 16Mhz do Arduino foi dividido por 256 com uso de prescaler (TCCR1B=0x04;)
 *   16000000/256=62500  corresponde a 16 microsegundos
 *   16000000/256/120=520  Freq Cristal / Prescalar / (2*60hz)
 *   Cade unidade do Timer1 corresponde a 16 microsegundos
 *   
 *   04/03/2019 - Sequencia de acionamento semiciclo negativo corrigido
 *   06/03/2019 - compatibilidade com RTC e datalogger mudando pino 9,10,11 para 5,6,7
 *                mudando pino do display CLK 7 DIO 6 para CLK 8 DIO 9
 *   03/04/2019 - Testando no lab eletricidade e ainda nao funcionou..              
 *                Quando desabilta a interrupcao funciona no comando manual
 *   12/04/2019 - Trocou TIL111 por 4N33 (melhorou o signal)             
 *              - Quando a largura_pulos_triac fica entre 430 e 166 a fase 2 fica instavel.
 *                ou seja.. quando a luz está mais fraca..  estah perdendo pulos negativos.. ou positivos
 *   19/04/2019 - Implementacao da chave para Dimmer ou Controle no Pino 10 no arduino
 *   13/11/2019 - Medindo tensao Vin proporcional a fonte para monitorar Tensao de fase RMS               
 *                Introduzindo bit de habilitacao da potencia hab_potencia
 *   13/11/2019 - Esta tendo interferencia pela porta serial na medicao da frequencia
 *                Vamos desabilitar a interrupcao no trecho critico do codigo
 *   15/11/2019 - Iniciar a mudanca para colocar o display LCD e Detector de sequencia de fase
 *   22/04/2020 - Mundanca para LCD ST7912 no modo texto
 *   
 */
 
//#include "TimerOne.h"

#include <avr/io.h>
#include <avr/interrupt.h>
//  #include "TM1637.h"
#include "U8glib.h"

// Pinos de medicao da frequencia da rede 
#define Interrupt_pin2 2

// Pinos de acionamento dos TRIACS
#define Fase_1_pin 5       
#define Fase_2_pin 6     
#define Fase_3_pin 7       

// Pinos do Display TM1636
//#define CLK 9 // para poder usar RTC mudar para 9  
//#define DIO 8 // para poder usar RTC mudar para 8 

// Pinos do LCD grafico a definir  // mudado 16/12/2019 com display na caixa
#define LCD_RW  9
#define LCD_RS  8 
#define LCD_En 11 

// Pinos de operacao e potenciometro 
#define Potenciometro1 A0
#define Chave_dimmer_controller 10  // No placa anterior a chave era no pino 10         

// Pino de leitura direcao sequencia de fase
#define sensor_sequencia_fase 12

// Pino de leitura da tensao da rede
#define Le_tensao A2   //  A leitura da entrada analigica A2 corresponde a tensao da rede 
                       //  220 Vac = 720 unidades 
                       //  200 Vac = 600 
                       //  150 Vac = 444 
                       //  100 Vac = 300   coincide com sem energia no Vin

#define Limiar_tensao 600

// Constantes do acionamento dos TRIACS

#define duracao_pulso 10 // microsegundos
#define LAMP_FORTE  70   // era 40 - mudou com osciloscopio - luminosidade forte com BT137   0 tics 
                         // este ponto de ajuste tem a ver com o ponto exato da int externo..
                         // vai ter que usar smith trigger para amarrar.                     
#define LAMP_FRACO 430   // luminosidade fraco com BT137 520 tics

// Variaveis do sistema

boolean hab_potencia=0;
boolean Debug_serial=1;
boolean Sequencia_fases=0;

volatile short int estado_x;
volatile unsigned int largura_pulso_triac, defasagem;

long int Referencia=6000;
long int Ganho=1;
long int Saida_calculada;
long int Erro;

unsigned int Tensao;  
unsigned long Frequencia=0; 

unsigned long tempo;
unsigned long tempo_ant;
unsigned long delta_tempo;

char Tensao_txt[6];
char Referencia_txt[6];
char Saida_calculada_txt[6];
char delta_tempo_txt[6];
char Frequencia_txt[6];
unsigned char tela=0;

U8GLIB_ST7920_128X64_4X u8g(LCD_En, LCD_RW, LCD_RS);   

void LCD_setup(void)
{  
 if ( u8g.getMode() == U8G_MODE_R3G3B2 ) {
    u8g.setColorIndex(255);     // white
  }
 else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) {
    u8g.setColorIndex(3);         // max intensity
  }
 else if ( u8g.getMode() == U8G_MODE_BW ) {
    u8g.setColorIndex(1);         // pixel on
  }
 else if ( u8g.getMode() == U8G_MODE_HICOLOR ) {
    u8g.setHiColorByRGB(255,255,255);
  }
 delay(100);
 u8g.firstPage();
  do {
    u8g.setFont(u8g_font_unifont);
    u8g.drawStr(20, 20, "Controlador"); 
    u8g.drawStr(40, 36, "Carga");      
    u8g.drawStr(20, 54, "16/12/2019");
  } while (u8g.nextPage());
 delay(2000);
}

char temp_str3[6];

void LCD_int(unsigned int a)
{   
 temp_str3[0]='0'+(char)(a/1000);
 a=a%1000;
 temp_str3[1]='0'+(char)(a/100);
 a=a%100; 
 temp_str3[2]='0'+(char)(a/10);
 a=a%10;
 temp_str3[3]='0'+(char)(a);
 temp_str3[4]=0;
} 

void LCD_long(long int a)
{   
 temp_str3[0]='0'+(char)(a/10000);
 a=a%10000;
 temp_str3[1]='0'+(char)(a/1000);
 a=a%1000;
 temp_str3[2]='0'+(char)(a/100);
 a=a%100; 
 temp_str3[3]='0'+(char)(a/10);
 a=a%10;
 temp_str3[4]='0'+(char)(a);
 temp_str3[5]=0;
} 


void Convert_to_string(void)
{
 unsigned char i; 
 LCD_int(Tensao);   strcpy(Tensao_txt, temp_str3); 
 LCD_int(Saida_calculada); strcpy(Saida_calculada_txt, temp_str3); 
 LCD_int(Frequencia); strcpy(Frequencia_txt, temp_str3);
 LCD_int(Referencia); strcpy(Referencia_txt, temp_str3); 
 LCD_long(delta_tempo); strcpy(delta_tempo_txt, temp_str3); 
}

void LCD_draw(void) {
 if (digitalRead(Chave_dimmer_controller)==HIGH) u8g.drawStr( 0, 10 , "Cntr"); 
  else u8g.drawStr(0, 10, "Dimm"); 
 u8g.drawStr( 45,10 , "V=");     u8g.drawStr(65, 10, Tensao_txt);
 if (Sequencia_fases) u8g.drawStr(102, 10, "ABC"); else u8g.drawStr(102, 10, "ACB");         
 u8g.drawStr( 0, 23 , "Freq ="); u8g.drawStr(50, 23, Frequencia_txt); 
        
 u8g.drawStr( 0, 36 , "Ref  ="); u8g.drawStr(50, 36, Referencia_txt); // u8g.drawStr(105, 36, "Rf"); 
 u8g.drawStr( 0, 49 , "Delta="); u8g.drawStr(50, 49, delta_tempo_txt);
 u8g.drawStr( 0, 62 , "Saida="); u8g.drawStr(50, 62, Saida_calculada_txt);
 switch(tela){
  case 0: u8g.drawStr(118,62 , "0"); break;
  case 1: u8g.drawStr(118,62 , "1"); break;
  case 2: u8g.drawStr(118,62 , "2"); break;
  case 3: u8g.drawStr(118,62 , "3"); break;
  case 4: u8g.drawStr(118,62 , "4"); break;
  case 5: u8g.drawStr(118,62 , "5"); break;  }
}

void  RS232_setup()
{ 
 Serial.begin(9600);
 while (!Serial) {  ; } // wait for serial port to connect. Needed for native USB port only
 Serial.println("# Dummy Load Control Chapada V1.2 @ Rudi 16/12/2019");
 Serial.println("# Referencia ,  Frequencia , Delta , Defasagem , Largura Pulso , Saida , Tensao");   
}

void setup()
{
  pinMode(Fase_1_pin, OUTPUT);
  pinMode(Fase_2_pin, OUTPUT);
  pinMode(Fase_3_pin, OUTPUT);
  pinMode(Interrupt_pin2,INPUT_PULLUP);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(Chave_dimmer_controller,INPUT_PULLUP); 
  pinMode(sensor_sequencia_fase, INPUT_PULLUP);
  hab_potencia=0;
  
  TIMSK1 = 0x03;    // 16.11.8 na datasheet 
  TCCR1A = 0x00;    // 16.11.1 na datasheet   desabilita inicialmente o timer:
  TCCR1B = 0x00;    // 16.11.2 na datasheet 
  attachInterrupt(digitalPinToInterrupt(2), interupcao_externa_INT0, RISING); // FALLING); //RISING); // CHANGE); 

  // espera 3 segundos para as primeiras medidas 
  
  delay(1000);
  // Faca a leitura do rele sequencia de fase para ajustar o sentido de chaveamento
  // Faca a leitura da tensao 
  Tensao=analogRead(Le_tensao); 
  // Calcule a frequencia 
  if (delta_tempo != 0) Frequencia= 100000000 / (delta_tempo); 
  Convert_to_string();
  LCD_setup();
  if (Debug_serial) RS232_setup();
}

void interupcao_externa_INT0()
{
  noInterrupts();  //deshabilita interrupcao 
  OCR1A=largura_pulso_triac;
  TCCR1B=0x04; // 16.11.2 - habilita o timer no modo que divide o clock real por 256
  TCNT1=0;     // 16.11.4 - zera o contador do timer
  estado_x=1;
  tempo=micros();    // calcula a frequencia da rede com timer 0
  delta_tempo = tempo-tempo_ant;
  tempo_ant=tempo;
  defasagem=(delta_tempo)/96;    // 2*3*16
  interrupts(); // habilita interrupcao de novo
  
  // para 60Hz delta tempo = 16666 microsegundos
  // 1/3 do tempo = 5555 microsegundos
  // metade da defasagem = 16666 / (2*3) = 2777 microsegundos
  // Cada ciclo do Timer corresponde a 1 / (16Mhz / 256) = 16 microsegundos
  // Defasagem = 2777 / 16 = 173 clicks do timer 
  // (delta_tempo / (2*3))/(256/16) = delta_tempo / 96
  //
  // calculo da frequencia aqui neste trecho ou na amostragem geral
  // if (delta_tempo != 0) Frequencia= 100000000 / (delta_tempo); 
}

ISR(TIMER1_COMPA_vect) { // chamada automaticamente sempre que o TCNT1 alcanca OCR1A.
 switch(estado_x)
 {
  case 0: break; 
  case 1: // sincronizou na fase 1 Semi ciclo positivo
          if (hab_potencia) digitalWrite(Fase_1_pin,HIGH);
          TCCR1B=0x04;
          TCNT1=0;   // zera o contador 
          OCR1A=defasagem;  
          delayMicroseconds(duracao_pulso);  // so funciona com pulso maior que 20 microsegundos
          digitalWrite(Fase_1_pin,LOW);
          // if (sequencia_fase==HIGH) estado_x=33; else estado_x=22;
          estado_x=33;   
          break;
  case 33: // sincronizou na fase 3 Semi ciclo negativo
          if (hab_potencia) digitalWrite(Fase_3_pin,HIGH);
          TCCR1B=0x04;
          TCNT1=0;   // zera o contador  
          OCR1A=defasagem;         
          delayMicroseconds(duracao_pulso);
          digitalWrite(Fase_3_pin,LOW);
          // if (sequencia_fase==HIGH) estado_x=2; else estado_x=0;
          estado_x=2;   
          break;
 case 2: // sincronizou na fase 2 Semi ciclo positivo
          if (hab_potencia) digitalWrite(Fase_2_pin,HIGH);
          TCCR1B=0x04;
          TCNT1=0;   // zera o contador          
          OCR1A=defasagem;          
          delayMicroseconds(duracao_pulso);
          digitalWrite(Fase_2_pin,LOW);
          // if (sequencia_fase==HIGH) estado_x=11; else estado_x=33;
          estado_x=11;   
          break;
  case 11: // sincronizou na fase 1 Semi ciclo negativo
          if (hab_potencia) digitalWrite(Fase_1_pin,HIGH);
          TCCR1B=0x04;
          TCNT1=0;   // zera o contador 
          OCR1A=defasagem;  
          delayMicroseconds(duracao_pulso);  // so funciona com pulso maior que 20 microsegundos
          digitalWrite(Fase_1_pin,LOW);
          // if (sequencia_fase==HIGH) estado_x=3; else estado_x=2;   
          estado_x=3;   
          break;
  case 3: // sincronizou na fase 3 Semi ciclo positivo
          if (hab_potencia) digitalWrite(Fase_3_pin,HIGH);
          TCCR1B=0x04;
          TCNT1=0;   // zera o contador          
          OCR1A=defasagem;          
          delayMicroseconds(duracao_pulso);
          digitalWrite(Fase_3_pin,LOW);
          // if (sequencia_fase==HIGH) estado_x=22; else estado_x=11;       
          estado_x=22;
          break;
   case 22: // sincronizou na fase 2 Semi ciclo negativo
          if (hab_potencia) digitalWrite(Fase_2_pin,HIGH);
          TCCR1B=0x04;
          TCNT1=0;   // zera o contador          
          OCR1A=defasagem;          
          delayMicroseconds(duracao_pulso);
          digitalWrite(Fase_2_pin,LOW);
          TCCR1B=0;
          TCNT1=0;
          // if (sequencia_fase==HIGH) estado_x=0; else estado_x=3;
          estado_x=0;   
          break;
  }
}

void loop()
{
  digitalWrite(LED_BUILTIN, HIGH);
  Convert_to_string();
  if (Debug_serial)
  { 
   Serial.print(Referencia);Serial.print(" , "); Serial.print(Frequencia);Serial.print(" , ");
   Serial.print(delta_tempo); Serial.print(" , "); Serial.print(defasagem);Serial.print(" , "); 
   Serial.print(largura_pulso_triac); Serial.print(" , "); Serial.print(Saida_calculada_txt);Serial.print(" , ");
   Serial.print(Tensao);Serial.println(" . "); 
  } 
  Tensao=analogRead(Le_tensao);   
  if (Tensao > Limiar_tensao) hab_potencia=1; else hab_potencia=0;
  if (delta_tempo != 0) Frequencia= 100000000 / (delta_tempo);  // converte para frequencia 4 digitos 
  if (digitalRead(sensor_sequencia_fase)==HIGH) Sequencia_fases=1; else Sequencia_fases=0; 
  if (digitalRead(Chave_dimmer_controller)==HIGH) 
  {
   // Rotina de controle 
   // Se a tensao for muito baixo inibe o triac
   Referencia = 6000 ;  // 60 Hz frequencia da rede
   // Ganho = analogRead(Potenciometro1)/10;
   Ganho = 1;
   Erro=Frequencia-Referencia;
   if (Erro < 0) { Saida_calculada=0; }
   else { Saida_calculada=Erro * Ganho;   }
  }
  else 
  { 
   // Rotina de Dimmer
   Referencia=analogRead(Potenciometro1); 
   Saida_calculada = Referencia; 
  } 

  if (Saida_calculada > 1024) Saida_calculada=1024;    
  if (Saida_calculada < 5) hab_potencia=0; else hab_potencia=1;  
  largura_pulso_triac=map(Saida_calculada,0,1024,LAMP_FRACO, LAMP_FORTE);  
  u8g.firstPage();  
  do {
      LCD_draw();   
  } while( u8g.nextPage() );
  if (tela++==5) tela=0;
  delay(100);   //500);
  digitalWrite(LED_BUILTIN, LOW);
}
 
