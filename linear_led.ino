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

  5 is very noticable duty cycle at pwm(50), when "flicking" the eye. cf 'e5'
  3 is only noticeable at fast eye flicker
  1 can't be detected by eye flicker

  We need a high ratio of direct-pwm to gated-pwm for low direct-pwm values:
    if 7812.5hz counter on direct pwm
    and 62.5KHz on gated
    then we only get 8 tics of the gated per tic of the direct
    so, we only can gate the direct at: 100%, 7/8, 6/8, etc.
    To get more "steps", you need a higher ratio.
  But, there is significant "change over" time if the direct-pwm is low:
    at 244hz counter, it takes 4ms to count from 0 to 255.
    If you switch from 2 to 1 for the direct, and 120 to 255 on the gated
    then worse case you'll see
      2 for 4 more ms, and 255 on the gated
      then 1 on direct, and 255 on gated, which is what you intended
      And, I see a "flash" about 1/2 the time
  So far, the only solution I see is to change the direct-pwm, and wait 4ms.
    That's not ideal, because now there will be up to 4ms when it's [1,120]
    before it goes to [1,255].
    The eye is less sensitive to "drop-outs" than "flashes" though.
    I vaguely remember something about timer-interrupts, but are they on the pwm timers?
    

PWM deltas are noticeable steps around 20.
But, start to become noticeable > 20, and definitely > 10. cf 'd1' 'd2'

PWM 1*1 is hard to see. but is visible. (cf 'B')

Decay-rate does seem to me to be visual "linear" in brightness (cf 'f'): constant brightness change
versus "nothing happens at 255->254, and accelerates as we get down to 50's" (cf 'F').
But, you can only get so far till n% == 1pwm.

Deal with < delta-n steps

What's the right decay rate? Didn't seem to notice stepping at 9%
*/

#include "tired_of_serial.h";

const unsigned long CLOCKHZ = 16000000;

const int led_direct = 9;
const int led_gater = 11; // diff timer, gates the direct led
const int fader_delay = 150;
const int step_delay = 2000 / 255;

void inline maximize_diff_divisor() {
  TCCR1B = TCCR1B & 0xF8 | 4; // pin 9, direct slowest rate. Not enough ratio till "4":"1" :: 1:256
  TCCR2B = TCCR2B & 0xF8 | 1; // pin 11, gater faster rate
  }
void inline default_divisors() {
  TCCR1B = TCCR1B & 0xF8 | 3;
  TCCR2B = TCCR2B & 0xF8 | 3;
  }

template<typename T, int sz> int size(T(&)[sz]) { return sz; }

namespace gfade5percent {
  // at 5% decay

  // do simple direct pwm until lowest value
  const byte direct_pwm[] = { 20,19,18,17,16,15,14,13,12,11,10, 0 };

  // for lowest...0, do the gated first, then dec the direct_pwm
  // fixme: these should just be counts in the direct_pwm list
  const byte gated_pwm9[] = { 242, 230, 0 };  // to get to pwm 9
  const byte gated_pwm8[] = { 242, 230, 0 };
  const byte gated_pwm7[] = { 242, 230, 0 };
  const byte gated_pwm6[] = { 242, 230, 219, 0 };
  const byte gated_pwm5[] = { 242, 230, 219, 0 };
  const byte gated_pwm4[] = { 242, 230, 219, 208, 0 };
  const byte gated_pwm3[] = { 242, 230, 219, 208, 198, 0 };
  const byte gated_pwm2[] = { 242, 230, 219, 208, 198, 188, 179, 0 };
  const byte gated_pwm1[] = { 242, 230, 219, 208, 198, 188, 179, 170,162,154,146,139, 123, 0 };
  const byte gated_pwm0[] = { 128, 0 }; // really, repeat the direct_pwm sequence

  // we know this size because it's the last value in direct_pwm..0
  const byte* gated_pwm[] = { gated_pwm0, gated_pwm1, gated_pwm2, gated_pwm3, gated_pwm4, gated_pwm5, gated_pwm6, gated_pwm7, gated_pwm8, gated_pwm9 };

  }

namespace gfade {
  // at 3% decay

  // do simple direct pwm until lowest value
  // 87 steps of direct pwm
  const byte direct_pwm[] = { 255,247,240,233,226,219,212,206,200,194,188,182,177,172,167,162,157,152,147,143,139,135,131,127,123,119,115,112,109,106,103,100,97,94,91,88,85,82,80,78,76,74,72,70,68,66,64,62,60,58,56,54,52,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17, 0 };

  // for lowest...0, do the gated first, then dec the direct_pwm

  // how many from the top of direct_pwm to use to transition from direct[x] to direct[x-1]
  // you can have more than we need here. [0] doesn't happen of course
  const int gated_pwm_counts[] = { 0, 88,23,14,10,8,6,6,5,4,4,4,3,3,3,3,3 };
  }

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

        case 'F' : // standard fade
          standard_fade(led_direct);
            command = 0xff;
            break;

        case 'f' : // explore decay-fade
          {
          print("decay percent 1..9: ");
          char c = readone();
          if ( c >= '1' && c <= '9' ) {
            float decay = (c - '0') / 100.0;
            print(F("decay @"));println(decay);
            decay_fade(decay, led_direct, 20);
            command = 0xff;
            }
          else {
            print("?");
            command='?';
            }
          }
          break;

        case 'd' : // decay fade w/gated at low end
          decay_bottom( led_direct );
          command = 0xff;
          break;

        case '2' : // fader 20..0..20
          fader(led_direct,20, fader_delay);
          command = 0xff;
          break;

        case 'G' : // fade 1 * 255..0..255
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

        case 'D': // fade over a decade
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
Serial.println(F("b  blink from pwm(1) to pwm(0) (vs. all/on off for pin 11"));
Serial.println(F("B  blink external led pin 9 and builtin 13"));
Serial.println(F("t  timers"));
Serial.println(F("F  standard fade"));
Serial.println(F("f  explore decay-fade"));
Serial.println(F("d  decay fade w/gated at low end"));
Serial.println(F("2  fader 20..0..20"));
Serial.println(F("G  fade 1 * 255..0..255"));
Serial.println(F("g  gater fader"));
Serial.println(F("e  test eye flicker at pwm rate-divisor"));
Serial.println(F("D  fade over a decade"));
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
  // fade from max..0..max
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

#include <math.h>

void decay_fade(float decay_rate, int pin, int step_delay) {
  // decay/grow at the rate, rounding to int, till delta is 0
  // so, may never get to zero. at 2%, stops around 25.
  byte fade[256] = {}; // force 0's

  // build decay table
  byte sofar = 255;
  int i =0;
  while (i <= 255) {
    fade[i] = round(sofar);
    sofar = round((float)sofar * (1.0 - decay_rate));
    // print("D ");print(fade[i]);print(F("->"));print(sofar);
    if (fade[i] == sofar) {
      print(F("  Range 255.."));print(sofar);print(F(" steps "));println(i);
      break; // done when it doesn't change
      }
    i++;
    }
  step_delay = 2000 / i; // approximate constant wavelength so it's comparable
  print(F("step delay "));println(step_delay);

  // run table (to 0, don't actually do zero!)
  while(1) {
    for(i=0; fade[i] != 0; i++) {
      analogWrite(pin, fade[i]);
      delay(step_delay);
      }
    if (Serial.available() > 0) break;
    i--;
    for(; i>=0; i--) {
      analogWrite(pin, fade[i]);
      delay(step_delay);
      }
    if (Serial.available() > 0) break;
    }

  }

void standard_fade(int pin) {
  
  int step_delay = 2000 / 255;

  // run table (to 0, don't actually do zero!)
  while(1) {
    for(int i=1; i<255; i++) {
      analogWrite(pin, i);
      delay(step_delay);
      }
    if (Serial.available() > 0) break;

    for(int i=255; i>1; i--) {
      analogWrite(pin, i);
      delay(step_delay);
      }
    if (Serial.available() > 0) break;
    }

  }

void decay_bottom(int pin) {
  // uses a decay table
  // at the bottom of the values (20 for 5% decay)
  // switch to a gated decay

  maximize_diff_divisor();

  // find the start index
  int start_at = 0;
  //for( start_at=0; gfade::direct_pwm[ start_at ] != 25; start_at++);

  int step_delay = 3000 / gfade::direct_pwm[ start_at ];

  byte direct_pwm_value;

  while(1) {
    // Down

    // the direct table goes from max..lowest-direct-pwm,0
    float current_percent = 1;
    float last_percent = 1;

    analogWrite(led_gater, 255); // make sure
    for( 
        byte* pwm = &gfade::direct_pwm[ start_at ];
        *pwm != 0; // zero is end. previous value is last direct-pwm
        pwm++
        ) {
      //break; //X
      direct_pwm_value = *pwm; // we need it after this loop
      analogWrite(pin, direct_pwm_value);
      current_percent = direct_pwm_value / 255.0;
      print(direct_pwm_value);print(F(" "));print( current_percent * 100 );print(F(" - "));println( current_percent / last_percent * 100);
      last_percent = current_percent;
      delay(step_delay);
      }
    direct_pwm_value -= 1;
    // direct_pwm_value is now the first value that needs to do gated got get to the next lower: direct_ct ... direct_ct-1
    //direct_pwm_value = 2; //X

    // for the rest of the direct pwm values, do a gated sequence on top, then decrement direct
    for( ; direct_pwm_value > 0; direct_pwm_value--) { // don't do zero yet
      int gated_count = gfade::gated_pwm_counts[ direct_pwm_value ];
      current_percent = direct_pwm_value / 255.0;
      print(direct_pwm_value); print(F(" ..."));print(gated_count); print(F(" "));println( current_percent * 100.0 );

      // will start at (direct_pwm_value, 255)
      analogWrite(pin, direct_pwm_value); // because the gated-fade gets us to direct_pwm_value-1
      delay(5); // wait for n -> n-1, because at 244hz, can take 4ms for pwm counter to finish previous count
      for(
          int i = 0; // start at (direct_pwm_value, 255)
          i < gated_count - 1;
          i++
          ) {
        byte gate_pwm = gfade::direct_pwm[i];
        //if (direct_pwm_value == 2 && i < 18) continue; //X
        analogWrite(led_gater, gate_pwm);
        current_percent = (direct_pwm_value / 255.0) * (gate_pwm / 255.0);
        print(direct_pwm_value);print(F(" @"));print(i);print(F(" g "));print(gate_pwm);
          print(F(" ")); print( current_percent * 100);print(F(" - ")); println( current_percent / last_percent * 100);
        last_percent = current_percent;
        delay( step_delay );
        //if ( direct_pwm_value == 1 && i > 0 ) { delay(500); break;} //X
        }

      }

    delay(300);
    if (Serial.available() > 0) break;
    }

  default_divisors();
  }
