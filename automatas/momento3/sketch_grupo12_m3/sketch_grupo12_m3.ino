/*
  Grupo #12: Proyecto 2. Sistema de acceso seguro para laboratorio con clave, alarma y 
  apertura motorizada

  Este sistema combina conceptos de seguridad, interfaz y lógica de estados
  La lógica del sistema funciona a través de 2 formas:
    - Acceso privilegiado usando un control IR
    - Acceso con keypad con usuario y contraseña
  
  Si un usuario no existe, se dispara una alarma del led y el buzzer
  Si se logra ingresar como un usuario pero la contraseña es incorrecta o
  corresponde a otro usuario se dispara tambien una alarma
  Si se comete un error en la contraseña 3 veces se bloquea el login y se envia
  un mensaje al usuario de acceso denegado.

  El servomotor vuelve a "cerrarse" o a girar despues de 10 segundos de tiempo fuera
  usando millis en vez de delay, asimismo el led y el buzzer se encienden un segundo
  usando el mismo método con millis

  Este proyecto simula un sistema de inicio de servicios para los objetos y varios
  sistemas de seguridad para el manejo de contraseñas, además de logs claros y
  organizados para ofrecer a un usuario una interfaz complementada a la que ofrece
  el LCD, estos logs también van con un estilo de journal junto al sistema de inicio.
*/

#include <LiquidCrystal_I2C.h>  // Librería necesaria para manejar el LCD I2C
#include <Servo.h>              // Librería necesaria para manejar el servomotor
#include <IRremote.hpp>         // Librería necesaria para manejar el receptor infrarrojo y el control remoto
#include <Keypad.h>             // Librería necesaria para el manejo del keypad 4x4

// Estados del sistema en enum
enum EstadoSistema {
  REPOSO,
  INGRESO,
  PERMITIDO,
  DENEGADO,
  BLOQUEO
};

EstadoSistema estado = REPOSO;

// Estructura para crear usuarios con un id o nombre de usuario y una contraseña
struct Usuario {
  String id;
  String passwd;
};

// Array que funciona como DataBase de usuarios
Usuario users[3] = {
  { "A198", "9*D2" },
  { "B123", "83A#" },
  { "C000", "AD23" }
};

// Definicion de ROWS y COLS = Tamaño del Keypad
const byte ROWS = 4;
const byte COLS = 4;

// Variables de Estado y Pines a usar en el montaje
const byte SERVO_PIN = 10;
const byte PIN_RECEPTOR = 11;
const byte PIN_LED = 13;
const byte PIN_LED_EXITO = 12;
const byte BUZZER = A0;
String usuario = "";
String password = "";
bool ingresandoPassword = false;
bool candadoCerrado = true;
bool usuarioExiste = false;
bool limpiarResiduo = false;
// Tiempos para medir con millis
unsigned long tAlerta = 0;
unsigned long tExito = 0;
unsigned long tBloqueo = 0;
unsigned long tMotor = 0;
// Contador de intentos fallidos
int fallidos = 0;
// Limites del servomotor para no llevarlo al límite de su torque
const int A_MAX = 170;
const int A_MIN = 10;

// Creacion de el mapa de teclas
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte rowPins[ROWS] = { 9, 8, 7, 6 };  // Pines a usar para las filas
byte colPins[COLS] = { 5, 4, 3, 2 };  // Pines a usar para las columnas

// Crear objeto keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo motor;

// Creacion de el LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Función de validación
bool validarUsuario(String user, String pass) {
  for (int i = 0; i < 3; i++) {
    if (users[i].id == user && users[i].passwd == pass) {
      return true;
    }
  }
  return false;
}

bool verificarExistencia(String user) {
  for (int i = 0; i < 3; i++) {
    if (users[i].id == user) {
      return true;
    }
  }
  return false;
}

void arduino_init() {
  /*
  Función que inicia todos los objetos y muestra un mensaje
  además ajusta el Serial y pone el modo para ciertos pines como LEDs
  y buzzer
  */
  Serial.begin(9600);
  ok(F("Serial iniciado en 9600 baudios..."));

  // Inicializador de el LCD
  lcd.init();
  lcd.backlight();
  ok(F("LCD iniciado y configurado..."));

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LED_EXITO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  ok(F("Pines de los LEDs y el Buzzer configurados como salida..."));

  motor.attach(SERVO_PIN);
  motor.write(A_MAX);  // Posicionar el candado inicialmente como cerrado
  ok(F("Servomotor atajado y configurado como cerrado..."));

  IrReceiver.begin(PIN_RECEPTOR, false);  // Inicializar el IR y poner el LED_FEEDBACK en false para no activar el pin 13 que ya lo tenemos ocupado
  ok(F("Receptor IR iniciado..."));
}

void setup() {
  arduino_init();  // Iniciar los objetos y pines una sola vez
  lcd_login();     // Mostrar mensajes en el LCD por primera vez
}

void loop() {
  control_super_usuario();  // Verificar ingresos de keypad para usuarios sin IR
  nopasswd();               // Verificar si alguien está ingresando sin contraseña con el IR
  verificacion();           // Verificamos al final del loop si hay un millis que ya haya llegado a su limite
}

void control_super_usuario() {
  /*
  Función que controla el acceso de usuarios autorizados (super usuario)
  a través de su nombre y su contraseña que son ingresados usando el keypad
  */
  char tecla = keypad.getKey();  // Obtener el estado del keypad

  if (tecla && estado != BLOQUEO) {  // Si hubo tecleo y no estamos bloqueados...
    estado = INGRESO;                // Cambiamos a INGRESO

    // Ingreso de Usuario
    if (!ingresandoPassword) {  // Sino estamos ingresando la contraseña

      usuario += tecla;  // Sumamos la tecla pulsada al usuario

      // Cuando el usuario tenga 4 caracteres
      if (usuario.length() == 4) {
        if (verificarExistencia(usuario)) {  // Verificamos su existencia
          ingresandoPassword = true;         // Pasamos a pedir la contraseña
          lcd.setCursor(0, 0);
          lcd.print(F("Ingrese clave  "));  // Espacios extra para sobreescribir el mensaje anterior
        } else {
          lcd.setCursor(0, 1);
          lcd.print(F("Error"));
          // Encendemos el LED de error, el buzzer y comenzamos a contar con millis
          digitalWrite(PIN_LED, HIGH);
          digitalWrite(BUZZER, HIGH);
          tAlerta = millis();
          fail(F("El usuario ingresado no existe"));
          usuario = "";  // Reiniciamos la variable de usuario
        }
        limpiarResiduo = true;  // Limpiar el residuo
      } else {
        lcd.setCursor(0, 1);
        lcd.print(usuario);  // Mostramos el usuario progresivamente en el LCD
      }
    }

    // Ingreso de contraseña
    else {
      password += tecla;

      // Escribimos un "*" por cada tecleo para ocultar la contraseña pero dar una señal al usuario de cuanto lleva ingresado
      lcd.setCursor(password.length() - 1, 1);
      lcd.print(F("*"));

      // Cuando la contraseña tenga 4 caracteres
      if (password.length() == 4) {
        if (validarUsuario(usuario, password)) {  // Validamos que la contraseña corresponda a dicho usuario
          lcd.setCursor(0, 1);
          lcd.print(F("Acceso OK"));
          ok(F("Acceso concedido"));
          estado = PERMITIDO;                 // Cambiamos a un acceso PERMITIDO
          motor.write(A_MIN);                 // Abrimos la cerradura
          candadoCerrado = false;             // Cambiamos el estado
          digitalWrite(PIN_LED_EXITO, HIGH);  // Mostramos una señal de exito
          // Iniciamos a contar para el motor y el LED de exito
          tMotor = millis();
          tExito = millis();
          ingresandoPassword = false;  // Cambiamos el estado para volver a la solicitud de usuario
        } else {                       // Si la contraseña era incorrecta
          lcd.setCursor(0, 1);
          lcd.print(F("Error    "));
          fail(F("Acceso denegado"));
          // Iniciamos señales de alarma
          digitalWrite(PIN_LED, HIGH);
          digitalWrite(BUZZER, HIGH);
          tAlerta = millis();     // Iniciar conteo de la alerta
          fallidos++;             // Aumentamos la cantidad de fallidos
          limpiarResiduo = true;  // Cambiamos para solicitar una limpieza de basura en el LCD
          estado = DENEGADO;      // Pasamos a un estado DENEGADO
        }

        if (estado == DENEGADO && fallidos == 3) {  // Si seguimos bloqueados y ya van 3 intentos fallidos de contraseña
          estado = BLOQUEO;                         // Cambiar estado
          tBloqueo = millis();                      // Empezar conteo del bloqueo
          fallidos = 0;                             // Reseteo para el próximo login
          ingresandoPassword = false;               // Cambiar estado de contraseña
          lcd.setCursor(0, 0);
          lcd.print(F("Bloqueo: Use IR"));
          lcd.setCursor(0, 1);
          lcd.print(F("o espere 10 sg"));
          limpiarResiduo = false;  // Asegurar que parte de nuestro mensaje no sea eliminado
        }

        // Reseteo del Sistema
        usuario = "";
        password = "";
      }
    }
  }
}

void nopasswd() {
  // Lectura y decodificación de señal del control remoto IR
  if (IrReceiver.decode()) {

    uint32_t codigo = IrReceiver.decodedIRData.command;

    if (codigo == 22 || codigo == 104) {  // Admitir dos codigos para controles físicos (22) y controles simulados en Wokwi (104)
      ok(F("Señar IR recibida."));
      if (estado == BLOQUEO) {  // Si estamos bloqueados la señal se considera un desbloqueo
        lcd.setCursor(0, 0);
        lcd.print(F("Desbloqueado    "));
      } else {  // Sino, simplemente se considera un acceso de administrador
        lcd.setCursor(0, 0);
        lcd.print(F("Admin acceso    "));
      }

      lcd.setCursor(0, 1);
      lcd.print(F("                "));
      limpiarResiduo = true;   // Ahora sí solicitamos limpieza
      motor.write(A_MIN);      // Y abrimos el servo en ambos casos
      candadoCerrado = false;  // Cambiamos el estado del servo

      digitalWrite(PIN_LED_EXITO, HIGH);  // Y mostramos señal de exito

      // Empezamos a contar
      tMotor = millis();
      tExito = millis();

      estado = PERMITIDO;  // Ponemos un estado de acceso PERMITIDO

      ok(F("Apertura por IR (admin)"));
    }
    IrReceiver.resume();  // Reiniciamos la lectura
  }
}

void lcd_login() {
  lcd.setCursor(0, 0);
  lcd.print(F("Ingrese usuario"));
}

void verificacion() {
  /*
  Función que verifica el tiempo transcurrido desde que se encendió la
  alarma o se giró el motor, se usa millis en vez de delay para que Arduino
  pueda seguir trabajando en otras tareas y no se bloquee
  */
  if ((estado == DENEGADO || estado == INGRESO || estado == BLOQUEO) && (millis() - tAlerta >= 1000)) {
    // Cambiamos si estamos en estado DENEGADO o de INGRESO para apagar la alerta
    digitalWrite(PIN_LED, LOW);
    digitalWrite(BUZZER, LOW);
    if (limpiarResiduo) {
      // Limpiamos solo si se nos indicó
      lcd.setCursor(0, 1);
      lcd.print(F("     "));
      limpiarResiduo = false;
    }
  }

  if (!candadoCerrado && (millis() - tMotor) >= 10000) {
    motor.write(A_MAX);  // Cerramos la cerradura tras 10 segundos
    candadoCerrado = true;
    Serial.println(F("Candado cerrado por seguridad trás 10 segundos"));
  }

  if (estado == BLOQUEO && millis() - tBloqueo >= 15000) {
    // Quitar el bloqueo tras 15 segundos y mostrar nuevamente el banner de login
    estado = REPOSO;
    lcd_login();
    lcd.setCursor(0, 1);
    lcd.print(F("              "));
  }

  if (estado == PERMITIDO && millis() - tExito >= 1000) {
    // Apagar el LED de exito, mostrar el login y volver al estado de reposo
    digitalWrite(PIN_LED_EXITO, LOW);
    lcd_login();
    estado = REPOSO;
    lcd.setCursor(0, 1);
    lcd.print(F("         "));
  }
}

/*
============================================
Funciones para debug en serial ok funciona para cuando algo se inicia correctamente
y fail para cuando se producen fallos y el usuario debe conocerlos.
*/
void ok(const __FlashStringHelper* msg) {
  Serial.print(F("[  OK  ] "));
  Serial.println(msg);
}

void fail(const __FlashStringHelper* msg) {
  Serial.print(F("[FAILED] "));
  Serial.println(msg);
}
