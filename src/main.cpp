//====================== Importar librerías ======================

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>

// vaiables conexion wifi
const char* ssid = "CLARO_FAMILY_GT";//CAMBIAR
const char* password = "MIDORISAIKA13";//CAMBIAR
const char* mqtt_server = "broker.emqx.io";
String rfid_copy = "";
char nombre[50];
char saldodb[50];
String valuename;
String valuesaldo;
float valsaldo;
bool nombreRecibido = false; 
bool saldoRecibido = false; 
bool esperandoRespuesta = false;
bool aprobacion = false;
bool esperandoSaldo = false;
unsigned long tiempoInicioEspera = 0;
const unsigned long tiempoMaxEspera = 5000; 
unsigned long startWaitTime;
bool isWaitingForResponse = false;
const unsigned long waitTimeout = 10000; 



boolean indetificadorAprobado = false;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido en el topico :");
  Serial.println(topic);
  //Serial.print("Mensaje: ");

  if(strcmp(topic, "inName") == 0){
   memset(nombre, 0, sizeof(nombre));
   memcpy(nombre, payload, length);
   //Serial.println(nombre);
   Serial.println();
   nombreRecibido = true;
  }
  if(strcmp(topic, "inSaldo") == 0){
   memcpy(saldodb, payload, length);
   Serial.println(saldodb);
   Serial.println();
   saldoRecibido = true;
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("esp32/rfid", "hello world");
      // ... and resubscribe
      client.subscribe("inName");
      client.subscribe("inSaldo");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//====================== Variables para el control del display LCD ======================

#define COLUMS 20
#define ROWS   4

#define PAGE   ((COLUMS) * (ROWS))

LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);

//====================== Variables para el control del módulo RFID ======================

#define SCK 18
#define MISO 19
#define MOSI 23

#define SS_PIN 5
#define RST_PIN 0

MFRC522 mfrc522(SS_PIN, RST_PIN);	// crea objeto mfrc522 enviando pines de slave select y reset

byte LecturaUID[4]; 				// crea array para almacenar el UID leido
byte Usuario1[4]= {0x4A, 0x32, 0x62, 0x17} ;    // UID de tarjeta leido en programa 1
byte Usuario2[4]= {0xB1, 0x7E, 0x46, 0x1D} ;    // UID de llavero leido en programa 1

//====================== Variables para el control de los LED's ======================

//Designamos nuestro pin de datos
#define PIN 33
//Designamos cuantos pixeles tenemos en nuestra cinta led RGB
#define NUMPIXELS 30

//Definimos el número de pixeles de la cinta y el pin de datos
//   Parámetro 1 = número de pixeles de la cinta
//   Parámetro 2 = número de pin de datos del Arduino
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

//Definimos nuestras variables de tipo uint32 para cada color que utilizaremos
// pixels.Color toma valores en RGB, desde 0,0,0 hasta 255,255,255
uint32_t rojo = pixels.Color(150,0,0);
uint32_t verde = pixels.Color(0,150,0);
uint32_t azul = pixels.Color(0,0,150);
uint32_t amarillo = pixels.Color(150,150,0);
uint32_t naranja = pixels.Color(255,150,0);
uint32_t fucsia = pixels.Color(150,0,150);
uint32_t nada = pixels.Color(0,0,0);

int p = 0; //Posición del led donde se encuentra el usuario

//====================== Defininos las variables para el control del Joystick analógico ======================

const int xAxis = 34; // Pin analógico del joystick para el eje X
const int yAxis = 35; // Pin analógico del joystick para el eje Y
const int push = 32;  // Pin del pulsador
int lecPush;

//====================== Variables para la inicialización del juego y control del menú ======================


int currentPosition = 0; // Índice actual del menú
int opcion = 0;

float saldo = 5.0;

//====================== Definimos las variables para la toma de decisiones ======================
int numerosAleatorios[4]={};
int LEDsAleatorios[30]={};
int ledUsuario[4] ={};
int cont = 0;

int elementosIguales[4]={};
int elementosDiferentes[4]={};

int aciertos = 0;
int desaciertos = 0;
int intentoExtra = 0;

float incremento = 0.0;

volatile bool interruptEnabled = true;

int delayval = 50; // Pausa de cincuenta milisegundos


//====================== Función interrupción ======================

void IRAM_ATTR handleButtonInterrupt() {
  if (cont < 4 && interruptEnabled) {
    int nuevoElemento = p;
    
    // Validar que el nuevo elemento no exista en la lista
    bool elementoDuplicado = false;
    for (int j = 0; j < cont; ++j) {
      if (ledUsuario[j] == nuevoElemento) {
        elementoDuplicado = true;
        break;
      }
    }

    if (!elementoDuplicado) {
      ledUsuario[cont] = nuevoElemento;
      cont++;
      if (cont == 4) {
        interruptEnabled = false;
        detachInterrupt(digitalPinToInterrupt(push));
      }
    }
  }
}

//====================== Función comparadora de identificador único ======================

boolean comparaUID(byte lectura[],byte usuario[])	// funcion comparaUID
{
  for (byte i=0; i < mfrc522.uid.size; i++){		// bucle recorre de a un byte por vez el UID
  if(lectura[i] != usuario[i])				// si byte de UID leido es distinto a usuario
    return(false);					// retorna falso
  }
  return(true);						// si los 4 bytes coinciden retorna verdadero
}
String bytesToHexString(byte lectura[], int length) {
    String result = "";

    for (int i = 0; i < length; i++) {
        // Convertir el byte a hexadecimal
        char hexCaracter[3]; // Dos caracteres hexadecimales y un terminador de cadena
        sprintf(hexCaracter, "%02X", lectura[i]); // %02X asegura que se imprima al menos dos dígitos, añadiendo un cero al principio si es necesario

        // Agregar a la cadena resultante
        result += String(hexCaracter);

        // Añadir un espacio entre los bytes, excepto después del último
        if (i < length - 1) {
            result += " ";
        }
    }

    return result;
}

//====================== Generar los LED's a adivinar ======================

void generarNumerosAleatoriosSinRepetir(int* lista, int longitud, int rango) {
  for (int i = 0; i < longitud; ++i) {
    // Generar un nuevo número aleatorio
    int nuevoNumero;
    do {
      nuevoNumero = random(rango);
    } while (std::find(lista, lista + i, nuevoNumero) != (lista + i));
    
    // Añadir el nuevo número a la lista
    lista[i] = nuevoNumero;
  }
}

//====================== Generar un ciclo de encendido y apagado de LED's ======================

void juegoDeLuces(int rango) {

//====================== Generar 4 números aleatorios del 0 al Número de pixeles ======================

  generarNumerosAleatoriosSinRepetir(LEDsAleatorios, rango, NUMPIXELS);

  for (int a = 0; a < rango; ++a) {
    pixels.setPixelColor(LEDsAleatorios[a], naranja);
    pixels.show(); 
    delay(100);
    pixels.setPixelColor(LEDsAleatorios[a], nada);
    pixels.show(); 

    /*pixels.setPixelColor(a, amarillo);
    pixels.setPixelColor(rango-1-a, amarillo);
    pixels.setPixelColor(rango-a, nada);
    pixels.setPixelColor(a-1, nada);

    pixels.show(); 
    delay(50);*/

    /*pixels.setPixelColor(ledUsuario[a], amarillo);
    pixels.setPixelColor(ledUsuario[rango-a], amarillo);
    pixels.show(); 
    delay(100);
    pixels.setPixelColor(ledUsuario[a], nada);
    pixels.setPixelColor(ledUsuario[rango-a], nada);
    pixels.show(); 
    delay(100);*/
  }
}

//====================== Verificar Aciertos ======================

void verificarAciertos(int indice){

  for (int j = 0; j < 4; ++j) {
   
    if (numerosAleatorios[indice] == ledUsuario[j]) {
      elementosIguales[aciertos] = ledUsuario[j];
      aciertos++;
      
    }
  }
}

//====================== Encender los LED's escogidos por el usuario ======================
void LEDsUsuario(int cont){
  if (cont > 0) {
    for (int j = 0; j < cont; ++j) {
      pixels.setPixelColor(ledUsuario[j], fucsia);
      pixels.show();
    }
  }
}

//====================== Función que controla el juego ======================

void juego() {

//====================== Establecemos la interrupción y enceramos las variables ======================

  interruptEnabled = true;
  attachInterrupt(digitalPinToInterrupt(push), handleButtonInterrupt, FALLING);
  cont = 0;
  p = 0;
  aciertos = 0;

//====================== Generar 4 números aleatorios del 0 al Número de pixeles ======================

  generarNumerosAleatoriosSinRepetir(numerosAleatorios, 4, NUMPIXELS);

  
  while (cont<4){

  //====================== Lectura y mapeo de los valores del Joystick (x,y) ======================

    int xVal = analogRead(xAxis); // Leer el valor del joystick en el eje X
    int yVal = analogRead(yAxis); // Leer el valor del joystick en el eje Y
    
    int newCursorY = map(yVal, 0, 4095, 100, -100);
    int newCursorX = map(xVal, 0, 4095, -100, 100);

    lecPush = digitalRead(push);

    Serial.print(" | Button: ");
    Serial.println(lecPush);
    Serial.print(" | newCursorX ");
    Serial.println(newCursorX);
    Serial.print(" | newCursorY: ");
    Serial.println(newCursorY);
    Serial.print(" | Led: ");
    Serial.println(p);
    Serial.println(" ");
  

  //====================== Actualizar la posición del LED donde se encuentra el usuario ======================

    while ( newCursorX >=-100 && newCursorX<=-70) {
      p++;
      if(p>NUMPIXELS-1){
        p = 0;
        //p = NUMPIXELS-1;
        pixels.setPixelColor(NUMPIXELS-1, nada);
        pixels.show();
        delay(50);
      }

      pixels.setPixelColor(p-1, nada);
      pixels.setPixelColor(p+1, nada);
      pixels.setPixelColor(p, azul); // Brillo moderado en azul

      pixels.show();   // Mostramos y actualizamos el color del pixel de nuestra cinta led RGB

      delay(delayval); // Pausa por un periodo de tiempo (en milisegundos).
      xVal = analogRead(xAxis);
      newCursorX = map(xVal, 0, 4095, -100, 100);
      delay(50);

      LEDsUsuario(cont);
    }

    while (newCursorX >=70 && newCursorX<=100) {
      p--;
      if(p<0){
        p = NUMPIXELS-1;
        //p = 0;
        pixels.setPixelColor(0, nada);
        pixels.show();
        delay(50);
      }

      pixels.setPixelColor(p-1, nada);
      pixels.setPixelColor(p+1, nada);
      pixels.setPixelColor(p, azul); // Brillo moderado en azul

      pixels.show();   // Mostramos y actualizamos el color del pixel de nuestra cinta led RGB

      delay(delayval); // Pausa por un periodo de tiempo (en milisegundos).
      xVal = analogRead(xAxis);
      newCursorX = map(xVal, 0, 4095, -100, 100);
      delay(50);

      LEDsUsuario(cont);
    }

    LEDsUsuario(cont);

  //====================== Mostrar en el Monitor Serial ======================

    for (int j = 0; j < cont; ++j) {
      Serial.print(" | Elemento Lista: ");
      Serial.println(ledUsuario[j]);
    }
    
    for (int j = 0; j < 4; ++j) {
      Serial.print(" | Número aleatorio: ");
      Serial.println(numerosAleatorios[j]);
    }

  }

//====================== Una vez que el usuario haya escogido sus 4 LED's ======================
  
  if (cont == 4){

  //====================== Mostrar juego de LED's como espera ======================
    /*int a = 0;
    for (int i = 0; i < 5; ++i) {
      juegoDeLuces(NUMPIXELS);
    }

    pixels.setPixelColor(a, nada);
    pixels.setPixelColor(NUMPIXELS-1-a, nada);
    pixels.show();
    delay(50);*/

  //====================== Apagamos los LED's ======================

    for (int e = 0; e < NUMPIXELS; ++e){
      pixels.setPixelColor(e, nada);
      pixels.show(); 
    }

  //====================== Se muestran los aciertos en lapsos de 1 segundo ======================
    for (int i = 0; i < 4; ++i) {
      verificarAciertos(i);
    }

    juegoDeLuces(10);
    for (int c = 0; c < aciertos; ++c){
      juegoDeLuces(30);
      pixels.setPixelColor(elementosIguales[c], verde);
      pixels.show(); 
      delay(500);
    }

  //====================== Se muestran los LED's no acertados y los incorrectos ======================
    for (int n = 0; n < 4; ++n){
      pixels.setPixelColor(numerosAleatorios[n], naranja);
      pixels.setPixelColor(ledUsuario[n], rojo);
    }

    for (int c = 0; c < aciertos; ++c){
      pixels.setPixelColor(elementosIguales[c], verde);
      Serial.print(" | Elementos iguales: ");
      Serial.println(elementosIguales[c]);
      
    }

  switch (aciertos) {
    case 0:
      if (client.connected()) {
      int aciertospu= 0;
      incremento = 0;
      String rfid_code = rfid_copy;
      String payload = rfid_code+","+aciertospu;
      snprintf(msg, MSG_BUFFER_SIZE, payload.c_str());
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("esp32/aciertos", msg);
      valsaldo=valsaldo+0.0;
    }
      break;

    case 1:
       if (client.connected()) {
      int aciertospu= 1;
      incremento = 0.5;
      String rfid_code = rfid_copy;
      String payload = rfid_code+","+aciertospu;
      snprintf(msg, MSG_BUFFER_SIZE, payload.c_str());
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("esp32/aciertos", msg);
      valsaldo=valsaldo+0.5;
    }

      break;

    case 2:
       if (client.connected()) {
      int aciertospu= 2;
      incremento = 1.5;
      String rfid_code = rfid_copy;
      String payload = rfid_code+","+aciertospu;
      snprintf(msg, MSG_BUFFER_SIZE, payload.c_str());
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("esp32/aciertos", msg);
      valsaldo=valsaldo+1.5;
    }
      
      break;
    
    case 3:
       if (client.connected()) {
      int aciertospu= 3;
      incremento = 1.5;
      String rfid_code = rfid_copy;
      String payload = rfid_code+","+aciertospu;
      snprintf(msg, MSG_BUFFER_SIZE, payload.c_str());
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("esp32/aciertos", msg);
      valsaldo=valsaldo+1.5;
    }
      
      break;
    
    case 4:
       if (client.connected()) {
      int aciertospu= 4;
      incremento = 2.0;
      String rfid_code = rfid_copy;
      String payload = rfid_code+","+aciertospu;
      snprintf(msg, MSG_BUFFER_SIZE, payload.c_str());
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("esp32/aciertos", msg);
      valsaldo=valsaldo+2;
    }
      
      break;

  }
    pixels.show(); 
    delay(50);
  
  }

}

//====================== Función para actualizar el cursor del menú ======================

void updateMenu(int currentPosition) {
  lcd.clear();
  lcd.setCursor(0, 0);

  switch (currentPosition) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Menu:");
      lcd.setCursor(0, 1);
      lcd.print("> Jugar");
      lcd.setCursor(0, 2);
      lcd.print("  Consultar Saldo");
      opcion = 0;
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Menu:");
      lcd.setCursor(0, 1);
      lcd.print("  Jugar");
      lcd.setCursor(0, 2);
      lcd.print("> Consultar Saldo");
      opcion = 1;
      break;

  }
}

void setup(){

//====================== Inicializamos la comunicación SPI y el lector MFRC522 ======================

  SPI.begin(SCK, MISO, MOSI, SS_PIN);				// inicializa bus SPI
  mfrc522.PCD_Init();			// inicializa modulo lector

//====================== Inicializamos nuestra cinta LED RGB ======================

  pixels.begin();

//====================== Establecemos el pulsador ======================

  pinMode(push, INPUT_PULLUP);
  

//====================== Establecemos la conexión con el Display LCD ======================

  while (lcd.begin(COLUMS, ROWS) != 1) //colums - 20, rows - 4
  {
    Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong. Only pins numbers: 4,5,6,16,11,12,13,14 are legal."));
    delay(5000);   
  }

  lcd.print(F("PCF8574 is OK..."));    //(F()) saves string to flash & keeps dynamic memory free
  delay(2000);

  lcd.clear();

  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop(){
    if (!client.connected()) {
    reconnect();
  }
  client.loop();
  do{
    Serial.print("AQUÍ 0");
    lcd.clear();
    lcd.setCursor(0, 0);
    delay(1000);
    lcd.setCursor(7, 1);
    lcd.print("Ingrese");
    lcd.setCursor(4, 2);
    lcd.print("Identificador");
    delay(1000);

  //====================== Entramos en un bucle hasta que el usuario ingrese el identificador ======================
    Serial.print("AQUÍ 1");
    if ( ! mfrc522.PICC_IsNewCardPresent())		// si no hay una tarjeta presente
      return;						// retorna al loop esperando por una tarjeta
    Serial.print("AQUÍ 2");
    if ( ! mfrc522.PICC_ReadCardSerial()) 		// si no puede obtener datos de la tarjeta
      return;						// retorna al loop esperando por otra tarjeta
        
    Serial.print("UID:");				// muestra texto UID:

  //====================== Imprimir en el monitor serial el identificador único de la tarjeta RFID leída ======================
    memset(LecturaUID, 0, sizeof(LecturaUID));
    for (byte i = 0; i < mfrc522.uid.size; i++) {	// bucle recorre de a un byte por vez el UID
        
      if (mfrc522.uid.uidByte[i] < 0x10){		// si el byte leido es menor a 0x10
        Serial.print(" 0");				// imprime espacio en blanco y numero cero
      }
        
      else{						// sino
        Serial.print(" ");				// imprime un espacio en blanco
      }
        
      Serial.print(mfrc522.uid.uidByte[i], HEX);   	// imprime el byte del UID leido en hexadecimal
      LecturaUID[i]=mfrc522.uid.uidByte[i];   	// almacena en array el byte del UID leido      
    }
        
    Serial.print("\t");   			// imprime un espacio de tabulacion   

  //====================== Comparamos que el usuario se encuentre en la base de datos ======================   
if (!esperandoRespuesta && client.connected()) {
    String rfid_code = bytesToHexString(LecturaUID, sizeof(LecturaUID) / sizeof(LecturaUID[0]));
    rfid_copy = rfid_code;
    String payload = rfid_code;
    snprintf(msg, MSG_BUFFER_SIZE, payload.c_str());
    client.publish("esp32/nombre", msg);
    esperandoRespuesta = true;
    startWaitTime = millis();
}

  // Si el nombre ha sido recibido y estábamos esperando una respuesta
  if (esperandoRespuesta) {
    if (millis() - startWaitTime > waitTimeout) {
        // El tiempo de espera se ha agotado
        esperandoRespuesta = false;
        // Manejar el timeout aquí, por ejemplo, volver a intentar o mostrar un mensaje de error
    }
    else if (nombreRecibido) {
    Serial.print("nombre recibido: ");
    Serial.println(nombre);
    String str = nombre;
    int index = str.indexOf(',');
    valuename= str.substring(0,index);
    valuesaldo = str.substring(index+1);
    valsaldo=valuesaldo.toFloat();
    Serial.println(valuename);
    Serial.println(valuesaldo);
    nombreRecibido = false;     // Resetea el flag para el próximo uso
    esperandoRespuesta = false; // Ya no estamos esperando una respuesta
                // Cambia o restablece la consulta según sea necesario
    if (strcmp(nombre, "no") != 0) {          
    lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("=====Bienvenid@=====");
      lcd.setCursor(0, 2);
      lcd.print("===");
      lcd.setCursor(5, 2);
      lcd.print(valuename);
      lcd.setCursor(15, 2);
      lcd.print("===");
      delay(2000);
      saldo = 0.0;
      indetificadorAprobado = true;
    }else if(strcmp(nombre, "no") == 0){
      Serial.println("No te conozco"); 		// muestra texto equivalente a acceso denegado
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("XXXXXX USUARIO XXXXX");
      lcd.setCursor(0, 2);
      lcd.print("XXXX DESCONOCIDO XXX");
      delay(2000);
            indetificadorAprobado = false;
    }
    }
  } 
 /* String user;
  
      user = usuario(LecturaUID);    
          
if (aprobacion) {	// llama a funcion comparaUID con Usuario1  
      Serial.println(user);	// si retorna verdadero muestra texto bienvenida
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("=====Bienvenid@=====");
      lcd.setCursor(0, 2);
      lcd.print(user);
      delay(2000);
      saldo = 0.0;
      aprobacion = false;
    }*/
    
    /*else{ if(comparaUID(LecturaUID, Usuario2)){	// llama a funcion comparaUID con Usuario2
            Serial.println("Bienvenida Jennifer");	// si retorna verdadero muestra texto bienvenida

            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("=====Bienvenida=====");
            lcd.setCursor(0, 2);
            lcd.print("======Jennifer======");
            delay(2000);
            saldo = 0.5;
            indetificadorAprobado = true;
          }*/
            
   /* else{					// si retorna falso
      Serial.println("No te conozco"); 		// muestra texto equivalente a acceso denegado
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("XXXXXX USUARIO XXXXX");
      lcd.setCursor(0, 2);
      lcd.print("XXXX DESCONOCIDO XXX");
      delay(2000);
            indetificadorAprobado = false;
    }*/
            
                      
    mfrc522.PICC_HaltA();  		// detiene comunicacion con tarjeta 

  } while (!indetificadorAprobado);

 
  lcd.clear();

  lcd.setCursor(5, 1);
  lcd.print("INICIANDO.");
  delay(1000);
  lcd.setCursor(5, 1);
  lcd.print("INICIANDO..");
  delay(1000);
  lcd.setCursor(5, 1);
  lcd.print("INICIANDO...");
  delay(1000);

  lcd.clear();

  while(indetificadorAprobado){
    //====================== Lectura y mapeo de los valores del Joystick (x,y) ======================

    int xVal = analogRead(xAxis); // Leer el valor del joystick en el eje X
    int yVal = analogRead(yAxis); // Leer el valor del joystick en el eje Y
      
    int newCursorY = map(yVal, 0, 4095, 100, -100);
    int newCursorX = map(xVal, 0, 4095, -100, 100);

    lecPush = digitalRead(push);

    // Ajusta los valores del joystick según tus necesidades
    if (newCursorY < -70) {
      currentPosition--;
      delay(200);  // Ajusta el retardo según tus necesidades
    } 
    else if (newCursorY > 70) {
      currentPosition++;
      delay(200);  // Ajusta el retardo según tus necesidades
    }

    // Limita la posición actual dentro del rango del menú
    currentPosition = constrain(currentPosition, 0, 1);

    // Actualiza el menú en la pantalla LCD
    updateMenu(currentPosition);

    delay(100);  // Puedes ajustar el retardo principal según tus 
    bool saldos=false;
    if ( lecPush == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);

      switch (opcion) {
        case 0:

            if (valsaldo >=0.50){

          //====================== Interfaz de Inicialización ======================

              lcd.setCursor(6, 1);
              lcd.print("Iniciando");
              lcd.setCursor(7, 2);
              lcd.print("Juego.");
              lcd.setCursor(6, 3);
              lcd.print("- $");
              lcd.setCursor(9, 3);
              lcd.print("0.50");
              delay(1000);
              lcd.setCursor(7, 2);
              lcd.print("Juego..");
              delay(1000);
              lcd.setCursor(7, 2);
              lcd.print("Juego...");
              delay(1000);

            //====================== Iniciamos los 3 intentos del usuario ======================

              valsaldo = valsaldo - 0.50;
              //jugar disminuir saldo
              if (client.connected()) {
                String rfid_code = rfid_copy;
                String payload = rfid_code;
                snprintf(msg, MSG_BUFFER_SIZE, payload.c_str());
                Serial.print("Publish message: ");
                Serial.println(msg);
                client.publish("esp32/jugar", msg);
            }
              for(int intento = 0; intento<3+intentoExtra; ++intento){

                lcd.clear();
                lcd.setCursor(2, 1);
                lcd.print("Juego en Curso");
                lcd.setCursor(5, 2);
                lcd.print("Intento:");
                lcd.setCursor(14, 2);
                lcd.print(intento+1);

              //====================== Damos paso al juego de LED's ======================

                pixels.setPixelColor(0, azul); //Indicador de que el juego ha empezado
                pixels.show(); 
                juego();

              //====================== Interfaz de Finalización ======================
                lcd.clear();
                lcd.setCursor(4, 1);
                lcd.print("Juego Acabado");
                lcd.setCursor(3, 2);
                lcd.print("Aciertos:");
                lcd.setCursor(14, 2);
                lcd.print(aciertos);
                lcd.setCursor(6, 3);
                lcd.print("+ $");
                lcd.setCursor(9, 3);
                lcd.print(incremento);
                delay(5000);

              //====================== Apagamos los LED's ======================
                for (int e = 0; e < NUMPIXELS; ++e){
                  pixels.setPixelColor(e, nada);
                  pixels.show(); 
                }
              
              //====================== Adicionar un intento si en su ultimo juego tuvo al menos un acierto ======================
                if(intento >= 2 && aciertos!=0){
                  lcd.clear();
                  lcd.setCursor(5, 1);
                  lcd.print("Ganaste un");
                  lcd.setCursor(5, 2);
                  lcd.print("Intento!!!");
                  delay(5000);
                  intentoExtra++;
              }

              }

            //====================== Interfaz de Reinicio ======================

              lcd.clear();
              lcd.setCursor(4, 1);
              lcd.print("Reiniciando");
              lcd.setCursor(6, 2);
              lcd.print("Juego.");
              delay(1000);
              lcd.setCursor(6, 2);
              lcd.print("Juego..");
              delay(1000);
              lcd.setCursor(6, 2);
              lcd.print("Juego...");
              delay(1000);

          }

      //====================== En caso de que el saldo sea insuficiente ======================

          else{
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("xxxxxxx Saldo xxxxxx");
            lcd.setCursor(0, 2);
            lcd.print("xxx Insuficiente xxx");
            delay(2000);

            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("Por favor, recargar");
            lcd.setCursor(5, 2);
            lcd.print("para jugar");
            delay(2000);
          }

          break;
        case 1:
          lcd.setCursor(7, 1);
          lcd.print("Saldo:");
          lcd.setCursor(7, 2);
          lcd.print("$");
          lcd.setCursor(8, 2);
          lcd.print(valsaldo);
          delay(2000);
          //indetificadorAprobado = false;
          break;

      }
    
    }
  }
}