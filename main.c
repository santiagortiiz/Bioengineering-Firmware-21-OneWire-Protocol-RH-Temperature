/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"

#define DATO_ESPERANDO_NUEVO_DATO 0
#define DATO_CORRECTO 1
#define DATO_INCORRECTO 2
#define POSITIVA 0
#define NEGATIVA 1
#define CHECKSUM 1
#define TIMEOUT 2
volatile uint8 cont=0;
volatile uint8 vector[40];
volatile uint8 DatosReady=DATO_ESPERANDO_NUEVO_DATO;

uint8 BandTimer = 0;
uint8 errorCheckSum = 0;
uint16 Contador = 0;
uint8 status = 0;
uint16 Humedad = 0;
uint16 Temperatura = 0;
uint8 contador_error_checksum = 0;
uint8 CheckSum = 0;
uint8 CheckSumMedido = 0;
uint8 VectorUnosCeros [40];
char SignoTemperatura = 0;

void IniciarSensor(void);
void CaptureData(void);
uint8 TimeOut(uint16);
uint16 GetHumedad(void);
uint8 GetCheckSum(void);
uint16 GetTemperatura(void);
uint8 CalcularCheckSum (void);
void ConvertirVector (void);
void MostrarError(char);
void MostrarVariables (void);
void  ReiniciarVariables(void);

uint8 bandera_test = 0;

CY_ISR_PROTO(Cronometro);
CY_ISR_PROTO(Input_Capture);
CY_ISR_PROTO(test);

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */
    isr_contador_StartEx(Cronometro);
    isr_timer_StartEx(Input_Capture);

    Contador_1_Start();
    LCD_Start();

    for(;;)
    {
        if(TimeOut(2500)){                              // Entra aquí cada 2500 milisegundos
            IniciarSensor();
            Timer_Start();
            CaptureData();                              // Bucle while hasta que sucedan los 2 eventos mencionados en CaptureData
            LED_Write(~LED_Read());                     // Indicador de funcionamiento
            
            if(DatosReady==DATO_CORRECTO){
                ConvertirVector();                      // Conversion de tiempos a 1s y 0s
                Humedad=GetHumedad();                   // Calculo de humedad
                CheckSum=GetCheckSum();                 // Adquisicion del Checksum que envia el sensor
                Temperatura=GetTemperatura();           // Calculo de temperatura
                CheckSumMedido=CalcularCheckSum();      // Calculo del Checksum propio
                DatosReady=DATO_ESPERANDO_NUEVO_DATO;   // Se reinicia el estado de DatosReady
                
                if(CheckSum != CheckSumMedido){         // Comparacion del Checksum propio con el recibido
                    MostrarError(CHECKSUM);             // Si hay un error por checksum se imprime el tipo de error
                    contador_error_checksum++;
                    if (contador_error_checksum == 4){
                        contador_error_checksum = 0;
                        CySoftwareReset();              // Reinicio del PSoC cuando hay 4 errores por Checksum
                    }
                }
                
                else{                                   // Si no hay problemas con el Checksum se imprimen
                    MostrarVariables();                 // las variables medidas
                }
            }
            
            else if(DatosReady==DATO_INCORRECTO){       // Si el sistema esta en el estado DATO_INCORRECTO
                MostrarError(TIMEOUT);                  // se muestra el error de timeout que indica que el 
                MostrarVariables();                     // dato no llego a tiempo
                DatosReady=DATO_ESPERANDO_NUEVO_DATO;   // y se reinicia el estado de DatosReady
                ReiniciarVariables();
            }

        }
        if(errorCheckSum==1){
            errorCheckSum=0;
            MostrarError(CHECKSUM);
            CyDelay(500);
            MostrarVariables();
            CySoftwareReset();
        }
    }
}

void IniciarSensor(void){   // Se envia la señal de inicio al sensor
    cont=0;                 // para que comience a enviar los datos
    Pin_Write(0);
    CyDelay(10);
    Pin_Write(1);
    CyDelayUs(10);
}

uint8 TimeOut(uint16 time){
    if(BandTimer){ 
        BandTimer=0;
        Contador++;

        if(Contador==time){     // Retorna un 1 cuando pase el tiempo
            Contador=0;         // en milisegundos que se establezca
            return 1;
            }
        else return 0;          // De lo contrario, siempre retorna un 0
    }
    else return 0;
    }

void CaptureData(void){
    while(DatosReady==DATO_ESPERANDO_NUEVO_DATO){   // Mientras este esperando un nuevo dato
        if(TimeOut(50)){                            // el sistema tiene 2 opciones:
            DatosReady=DATO_INCORRECTO;             // - Que pasen 50 milisegundos y se acive DATO_INCORRECTO
            Timer_Stop();                           // - Que se active DATO_CORRECTO luego de 
        }                                           // recibir 42 flancos de bajada en el Timer
    }
}

void ConvertirVector (void){
    for(uint8 i=0; i<40; i++){                                          // Recorre el vector que almaceno los tiempos
        if(vector[i]>65 && vector[i]<95) VectorUnosCeros[i]=0;          // capturados por el Timer y los convierte a
        else if(vector[i]>105 && vector[i]<140) VectorUnosCeros[i]=1;   // 1s y 0s en funcion de cada valor
    }
}

uint16 GetHumedad(void){
    uint16 H=0;
    for(uint8 i=0; i<16; i++){              // Se almacenan los bits recibidos de la humedad
        if(VectorUnosCeros[i]==0) H=H<<1;   // como si fuera un shift register, si el valor
        else if(VectorUnosCeros[i]==1){     // corresponde a un 0 se hace un corrimiento de bits
            H=H<<1;                         // a la izquierda, y si es un 1, se hace un corrimiento
            H++;                            // y se aumenta en 1
        }
    }
    return H;
}

uint16 GetTemperatura(void){                // La primera parte es analoga a la funcion
    uint16 T=0;                             // GetHumedad
    for(uint8 i =0;i<16;i++){
        if(VectorUnosCeros[i+16]==0) T=T<<1;
        else if(VectorUnosCeros[i+16]==1){
            T=T<<1;
            T++;
        }
    }
                                            // Sin embargo dado que la temperatura puede ser
    T=T&0x7FFF;                             // negativa, se realiza una mascara con:
                                            // 0111 1111 1111 1111
                                            // y se activa una bandera con el signo de la T
    if(VectorUnosCeros[16]==0)SignoTemperatura=POSITIVA;    // segun el valor de la 16ava
    else SignoTemperatura=NEGATIVA;         // posicion de VectorUnosCeros
    return T;                               // finalmente se retorna el valor absoluto de T
}

uint8 GetCheckSum(void){
    uint8 CS=0;                                     // Se adquieren los bits del Checksum
    for(uint8 i =0; i<8; i++){                      // recibidos del sensor como un
        if(VectorUnosCeros[i+32]==0) CS=CS<<1;      // Shift Register
        else if(VectorUnosCeros[i+32]==1){
            CS=CS<<1;
            CS++;
        }
    }
    return CS;
}

uint8 CalcularCheckSum (void){                      // Se calcula el checksum con los valores
    uint8 Chks=0;                                   // de RH y T obtenidos
    Chks=(uint8)(Humedad>>8)+(uint8)(Humedad)+(uint8)(Temperatura>>8)+(uint8)(Temperatura);
    if(SignoTemperatura==NEGATIVA)Chks+=128;
    return Chks;
}

void MostrarError(char id){
    LCD_ClearDisplay();
    if (id == 1)LCD_PrintString("Error de CHECKSUM");
    else LCD_PrintString("Error de TIMEOUT");
    CyDelay(200);
}

void MostrarVariables (void){
    LCD_ClearDisplay();
    LCD_Position(0,0);
    LCD_PrintString("Cks = ");
    LCD_PrintNumber(CheckSum);
    LCD_Position(0,10);
    LCD_PrintString("CksM = ");
    LCD_PrintNumber(CheckSumMedido);
    LCD_Position(2,0);
    LCD_PrintString("Temp = ");
    LCD_PrintNumber(Temperatura/10);
    LCD_PutChar(',');
    LCD_PrintNumber(Temperatura%10);
    LCD_PutChar(LCD_CUSTOM_0);
    LCD_PutChar('C');
    LCD_Position(3,0);
    LCD_PrintString("Humedad = ");
    LCD_PrintNumber(Humedad/10);
    LCD_PutChar(',');
    LCD_PrintNumber(Humedad%10);
    LCD_PutChar('%');
}

void  ReiniciarVariables(void){
    BandTimer = 0;
    errorCheckSum = 0;
    Contador = 0;
    status = 0;
    Humedad = 0;
    Temperatura = 0;
    CheckSum = 0;
    CheckSumMedido = 0;
}

CY_ISR(Cronometro){
    BandTimer=1;                                        // Activa la bandera cada milisegundo
}
   
CY_ISR(Input_Capture){
    status=Timer_ReadStatusRegister();
    if(status & Timer_STATUS_CAPTURE){                  // Entra cada flanco de bajada
        cont++;
        if(cont<3){
            Timer_ReadCapture();                        // Se lee el valor del flanco de bajada
            Timer_WriteCounter(65535);                  // pero los primeros 3 no interesan
        }
        else{                                           // Cuando contador ya sea 3 se calculan los conteos
            vector[cont-3]=65535-Timer_ReadCapture();   // hasta el flanco de bajada y se almacenan en el
            Timer_WriteCounter(65535);                  // vector de 40 posiciones
            if(cont>=42){                               // Cuando el contador sea 42, el vector estara lleno
                cont=0;                                 // y se activa la bandera DATO_CORRECTO.
                Timer_Stop();                           // Finalmente se detiene el Timer
                DatosReady=DATO_CORRECTO;
            }
        }
    }
}

/* [] END OF FILE */
