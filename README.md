# Controlador de carga para pico central hidrelétrica
Rudivels@26/03/2020

Programa para Arduino para controlar a carga de um pico central hidrelétrico trifasico por meio de triacs acionando uma carga de lastro resistiva.

*Arduino 3 plase Load controller with resistive dummy*


# 1. Apresentação 
Este programa faz parte de um projeto de um central de monitoramento e comando de uma unidade geradora de energia elétrica numa localidade remota. 
A unidade geradora é um pico central hidrelétrica composto por uma turbina hidraulica e um gerador elétrica. Estratégias de controle centrais hidrelétricas.  

O central tem que perimitir a configuração e/ou reprogramação remota de um controlador de carga dessa micro unidade geradora de energia elétrica que é implementado por meio de um microcontrolador Arduino. 

Além disso o central tem que monitorar a geração de energia elétrica e sua qualidade (potência, energia, fator de potência, interrupções de fornecimento, etc), e mandar essas informações via internet para um computador central onde será intergrada num sistema Supervisório Control and Data Aquisition (Scada). Este monitoramente está descrito em <https://github.com/rudivels/MicroHydro_Scada>

O diagrama de bloco de todo o sistema é mostrada no figura a seguir.

<img src="Diagrama_blocos_Arduin.jpg" alt="Diagrama" title="Diagrama" width="600"  height="450" />

Um foto do hardware montado com o Arduino, Display é mostrada a seguir.


<img src="foto_control_carga.jpg" alt="Quadro" title="Quadro" width="300"  height="450" />

.


Este respositório detalhará a implementação do controlador de carga no Arduino, e a estrutura de configuração e atualização remota no Raspberry. 


## 2. Controle de potência de 


## 2. Implementação 

O hardware usado na implementação do controlador de carga tem os seguintes componentes:
- Arduino Uno (Nano)
- Banco de triacs opto isolado para carga trifásica
- display LCD ST7920
- Raspberry

O principio de funcionamento do controlador é de manter sempre a carga aplicada a unidade geradora constante. Neste caso a unidade geradora é um pico central hidrelétrica com uma turbina Pelton que aciona um gerador trifasico sincrono de 1800 rpm @ 60hz de 3 KVA. 

O esquemático da placa de acionamento e controle 
