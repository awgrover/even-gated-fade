/*

Low 3 bits are cpu clock divisor for PWM rate
UNO is 16MHz
5,6 TCCR0B, also delay(), and millis() (?)
9,10 TCCR1B, default 500Hz, /64
3,11 TCCR2B, default 500Hz

divisors allowed values:
  001 = 1 -> 1 / 256 = 62.5KHz
  010 = 2 -> 8 / 256 = 7812.5Hz
  011 = 3 -> 64 / 256 = 976.5625 DEFAULT
  100 = 4 -> 256 / 256 = 244.14
  101 = 5 -> 1024 / 256 = 61.035

  5 is very noticable duty cycle at pwm(50) cf 'e5'
  3 is only noticeable at fast eye flicker
  1 can't be detected by eye flicker

PWM deltas are noticeable steps around 20.
But, start to become noticeable > 20, and definitely > 10. cf 'd1' 'd2'

PWM 1*1 is hard to see. but is visible. (cf 'B')

*/

#include "tired_of_serial.h";

const unsigned long CLOCKHZ = 16000000;

const int led_direct = 9;
const int led_gater = 11; // diff timer, gates the direct led
const int fader_delay = 150;

#define maximize_diff_divisor \
  TCCR1B = TCCR1B & 0xF8 | 4; /* pin 9, direct slowest rate */ \
  TCCR2B = TCCR2B & 0xF8 | 1; /* pin 11, gater faster rate */
#define default_divisors \
  TCCR1B = TCCR1B & 0xF8 | 3; \
  TCCR2B = TCCR2B & 0xF8 | 3;

void setup() {
  Serial.begin(115200);
  print("Start ? for menu");println();
  pinMode( led_direct, OUTPUT);
  pinMode( led_gater, OUTPUT);
  pinMode( LED_BUILTIN, OUTPUT);
  analogWrite(led_gater, 255);
  }

void loop() {
    static char command = 'G';

    switch (command) {
        // set command to '?' to display menu w/prompt
        // set command to 0xff to prompt
        // set command to 0xfe to just get input
        // could do your "loop" in while(Serial.available() <= 0) { dosomething }

        case 'b': // blink from pwm(1) to pwm(0) (vs. all/on off for pin 11
            pwm1percent_blink(led_direct, 255, led_gater);
            command = 0xff;
            break;

        case 'B': // blink external led pin 9 and builtin 13
            pwm1percent_blink(led_direct, 1, led_gater);
            blink(led_direct, LED_BUILTIN);
            command = 0xff;
            break;

        case 't' : // timers
          show_timers();
          command = '?';
          break;

        case 'f' : // fader
          fader(led_direct,20, fader_delay);
          command = 0xff;
          break;

        case 'G' : // gater on pwm(1)
          TCCR1B = TCCR1B & 0xF8 | 4; // pin 9, direct slowest rate
          TCCR2B = TCCR2B & 0xF8 | 1; // pin 11, gater faster rate
          analogWrite(led_direct, 1);
          fader(led_gater,255, 5);
          analogWrite(led_gater, 255);
          TCCR1B = TCCR1B & 0xF8 | 3; // pin 9, direct slowest rate
          TCCR2B = TCCR2B & 0xF8 | 3; // pin 11, gater faster rate
          command = 0xff;
          break;

        case 'g' : // gater fader
          gater_fader(20);
          analogWrite(led_gater, 255);
          command = 0xff;
          break;

        case 'e': // test eye flicker at pwm rate-divisor
          {
          Serial.print(F("1..5 for divisor, q "));
          char subcommand;
          switch (subcommand = readone()) {
            case '1': case '2': case '3': case '4': case '5': 
              eyeflicker( subcommand - '0' );
              command = 0xff;
              break;
            case '0': case 'q': case 'x':
              command = '?';
              break;
            default:
              Serial.println(F("?"));
              command = '?';
              break;
            }
          }
          break;

        case 'd': // fade over a decade
          {
          print(F("0..9 for decade "));
          char subcommand;
          switch (subcommand = readone()) {
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
              decade_fade(subcommand - '0');
              command = 0xff;
              break;
            case 'q': case 'x':
              command = 0xff;
              break;
            default:
              Serial.println(F("?"));
              command = '?';
              break;
            }
          }
          break;
      
      case '?':
      // menu made by: use the menu.mk: make menu
Serial.println(F("b  blink external led pin 9 and builtin 13"));
Serial.println(F("B  blink from pwm(1) to pwm(0)"));
Serial.println(F("t  timers"));
Serial.println(F("f  fader"));
Serial.println(F("G  gater on pwm(1)"));
Serial.println(F("g  gater fader"));
Serial.println(F("e  test eye flicker at pwm rate-divisor"));
Serial.println(F("d  fade over a decade"));
      // end menu

      // fallthrough

      case 0xff : case -1: // show prompt, get input
        Serial.print(F("Choose (? for help): "));
        // fallthrough

      case 0xfe : case -2: // just get input
        while(Serial.available() <= 0) {}
        command = Serial.read();
        Serial.println(command);
        break;

      default : // show help if not understood
        print("what? ");print(int(command));println();
        delay(10); while(Serial.available() > 0) { Serial.read(); delay(10); } // empty buffer
        command = '?';
        break;

      }
    }

char readone() {
  while(Serial.available() <= 0) {};
  char v= Serial.read();
  Serial.println(v);
  return v;
  }

void blink(int pin, int otherpin) {
  print("blink ");print(pin);println();
  while(Serial.available() <= 0) {
    digitalWrite(pin, HIGH);
    digitalWrite(otherpin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    digitalWrite(otherpin, LOW);
    delay(100);
    }
  }

void pwm1percent_blink(int pin, int otherpwm, int otherpin) {
  print("blink * ");print(otherpwm);print(" ");print(pin);println();

  maximize_diff_divisor;

  while(Serial.available() <= 0) {
    digitalWrite(LED_BUILTIN,HIGH);
    analogWrite(pin, 1);
    analogWrite(otherpin, otherpwm);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
    digitalWrite(pin, LOW);
    digitalWrite(otherpin, LOW);
    delay(100);
    }
  
  default_divisors;
  }

void decade_fade(byte decade) {
  const int delayms = 150;
  const byte min_range = decade * 10;
  const byte max_range = min_range +3;

  print("fade ");print(max_range);print(" ");print(min_range);println();

  unsigned long start = millis();
  int cycles;

  while(1) {
    cycles++;
    for(byte i = max_range; i > min_range; i--) {
      analogWrite( led_direct, i);
      if (Serial.available() > 0)
        { print(  (millis()-start) / cycles ); print(" millis/cyc");println(); return; }
      delay(delayms);
      }
    for(byte i = min_range; i < max_range; i++) {
      analogWrite( led_direct, i);
      if (Serial.available() > 0)
        { print(  (millis()-start) / cycles ); print(" millis/cyc");println(); return; }
      delay(delayms);
      }
    }

  }

void show_timers() {
  print("Timers:");println();
  show_timer(0,"5/6/millis/delay", TCCR0B);
  show_timer(1,"9/10", TCCR1B);
  show_timer(2,"3/11", TCCR2B);
  println();
  }

void show_timer(int name, char * reason, unsigned int t) {
  print(name);print(" ");print(reason);print(" 0b");
  print_bits(t); print(" "); print( t & 0b111 ); 
  print(" ");print(CLOCKHZ / (t & 0b111));print("Hz");
  println();
  }

void print_bits(unsigned int x) {
  print_bits( (byte) ( (x & 0xFF00) >> 8 ));
  print('.');
  print_bits( (byte) (x & 0xFF) );
  }
void print_bits(byte x) {
  print(x & B10000000 ? 1 : 0);print(x&B1000000 ? 1 : 0);print(x&B100000 ? 1 : 0);print(x&B10000 ? 1 : 0);
  print('.');
  print(x&B1000 ? 1 : 0);print(x&B100 ? 1 : 0);print(x&B10 ? 1 : 0);print(x&B1 ? 1 : 0);
  }

void eyeflicker(byte divisor) {
  // max flicker effect, try eye flickering to detect
  TCCR1B = TCCR1B & 0xF8 | divisor; 
  analogWrite(led_direct, 10); // max flicker
  }
  
void fader(int pin, byte max, int step_delay) {
  while(1) {
    for (int p=max; p>0; p--) {
      analogWrite(pin, p);
      delay(step_delay);
      }
    if (Serial.available() > 0) break;

    for (int p=0; p<max; p++) {
      analogWrite(pin, p);
      delay(step_delay);
      }
    if (Serial.available() > 0) break;
    }
  }


void gater_fader(byte max) {
  TCCR1B = TCCR1B & 0xF8 | 4; // pin 9, direct slowest rate
  TCCR2B = TCCR2B & 0xF8 | 1; // pin 11, gater faster rate

  while(1) {
    for (int p=max; p>0; p--) {
      analogWrite(led_direct, p);

      // 255, 128, 64, 32
      analogWrite(led_gater, 255);
      delay(fader_delay/4);
      analogWrite(led_gater, 230);
      delay(fader_delay/4);
      analogWrite(led_gater, 207);
      delay(fader_delay/4);
      analogWrite(led_gater, 180);
      delay(fader_delay/4);
      }
    if (Serial.available() > 0) break;

    for (int p=0; p<max; p++) {
      analogWrite(led_direct, p);

      analogWrite(led_gater, 162);
      delay(fader_delay/4);
      analogWrite(led_gater, 180);
      delay(fader_delay/4);
      analogWrite(led_gater, 207);
      delay(fader_delay/4);
      analogWrite(led_gater, 230);
      delay(fader_delay/4);
      }
    if (Serial.available() > 0) break;
    }

  TCCR1B = TCCR1B & 0xF8 | 3;
  TCCR2B = TCCR2B & 0xF8 | 3;
  }


