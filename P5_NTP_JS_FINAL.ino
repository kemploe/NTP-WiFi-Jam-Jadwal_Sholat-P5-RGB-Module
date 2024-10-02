// NodeMCU ESP8266 - P5 NTP CLOCK

#include <ESP8266WiFi.h>
#include <ezTime.h>
#include <PxMatrix.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

#include <Fonts/Org_01.h>
#include <Fonts/Picopixel.h>
#include <Fonts/Tiny3x3a2pt7b.h>
#include <Fonts/TomThumb.h>

// Adjustable parameters
const char * sketchName = "P5_NTP_JS_FINAL";
const char     * t_zone = "Asia/Jakarta";     // See https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
const char   * ntp_pool = "id.pool.ntp.org";  // NTP Server pool address
const int       id_kota = 1301;               // See https://api.myquran.com/v2/sholat/kota/semua
const int    brightness = 30;                 // Display brightness level
String     new_hostname = "p5-ntp-js";

// Buffers for string to character conversion from JSON payload
char b_imsak[10], b_subuh[10], b_terbit[10], b_dhuha[10], b_dzuhur[10], b_ashar[10], b_maghrib[10], b_isya[10];

// day of the week (dow) and it's absolute x position array
char dow_matrix[7][10] = {"AHAD","SENIN","SELASA","RABU","KAMIS","JUMAT","SABTU"};
// byte dow_x_pos[7] = {48, 44, 40, 48, 44, 44, 44};
// char loc_dow_array[7][10] = {"AHD","SEN","SEL","RAB","KAM","JUM","SAB"};
// byte x_pos[7] = {16, 16, 16, 16, 16, 16, 16};
// char loc_dow_array[7][10] = {"SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"};
// byte x_pos[7] = {13, 13, 12, 10, 11, 13, 11};
// char loc_dow_array[7][10] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
// byte x_pos[7] = {16, 16, 16, 16, 16, 16, 16};
static byte prev_loc_dow = 0;

int prev_hour = -1;
int prev_minute = -1;
int sync_result, sync_age;

Timezone my_tz;                                            // local timezone variable
time_t loc_time;                                           // current & displayed local time

Ticker display_ticker;

// NTP Sync definitions
// NTP Sync Status displayed as clock separator
#define SYNC_MARGINAL 3600                                 // yellow status if no sync for 1 hour
#define SYNC_LOST 86400                                    // red status if no sync for 1 day

// Pin assignment for LED MATRIX
#define P_A     5           // pin A    on LED MATRIX is connected to GPIO   5
#define P_B     4           // pin B    on LED MATRIX is connected to GPIO   4
#define P_C     15          // pin C    on LED MATRIX is connected to GPIO  15
#define P_D     12          // pin D    on LED MATRIX is connected to GPIO  12
#define P_E     0           // for 1/32 scan only
#define P_LAT   16          // pin LAT  on LED MATRIX is connected to GPIO  16
#define P_OE    2           // pin OE   on LED MATRIX is connected to GPIO   2

#define p5_width 64
#define p5_height 32

// This defines the 'on' time of the display in us (microsecond). 
// The larger this number, the brighter the display.
// If too large the ESP will crash
uint8_t display_draw_time = 40;                            //30-70 is usually fine

PxMATRIX p5_display (64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D);

// COLOR DEFINITION
uint16_t p5_BLACK     = p5_display.color565(  0,   0,   0);
uint16_t p5_WHITE     = p5_display.color565(255, 255, 255);
uint16_t p5_RED       = p5_display.color565(255,   0,   0);
uint16_t p5_DMRED     = p5_display.color565(96,    0,   0);
uint16_t p5_GREEN     = p5_display.color565(  0, 255,   0);
uint16_t p5_DMGREEN   = p5_display.color565(  0, 128,   0);
uint16_t p5_BLUE      = p5_display.color565(  0,   0, 255);
uint16_t p5_YELLOW    = p5_display.color565(255, 255,   0);
uint16_t p5_DMYELLOW  = p5_display.color565(255, 255,   0);
uint16_t p5_CYAN      = p5_display.color565(  0, 255, 255);
uint16_t p5_DMCYAN    = p5_display.color565(  0, 128, 128);
uint16_t p5_GREY      = p5_display.color565(128, 128, 128);
uint16_t p5_DMGREY    = p5_display.color565(96,   96,  96);
uint16_t p5_MAGENTA   = p5_display.color565(255,   0, 255);
uint16_t p5_DMMAGENTA = p5_display.color565(96,    0,  96);

// DISPLAY_UPDATER FUNCTION
void display_updater()
{
  p5_display.display(display_draw_time);
}

void display_update_enable(bool is_enable)
{
  if (is_enable)
    display_ticker.attach(0.004, display_updater);
  else
    display_ticker.detach();
}

// STATIC_DISPLAY FUNCTION
void static_display()
{
  p5_display.setFont( &TomThumb );

  // display praying time header
  p5_display.setTextSize(1);                               // size 1 == 8 pixels high 6 pixels wide
  p5_display.setTextColor(p5_DMCYAN);
  p5_display.setCursor(0, 25);                             // start at x=3 y=19, with 8 pixel of spacing
  p5_display.print( "S" );

  p5_display.setCursor(22, 25);                            // start at x=38 y=19, with 8 pixel of spacing
  p5_display.print( "T" );

  p5_display.setCursor(44, 25);                            // start at x=3 y=25, with 8 pixel of spacing
  p5_display.print( "D" );

  p5_display.setCursor(0, 31);                             // start at x=38 y=25, with 8 pixel of spacing
  p5_display.print( "A" );

  p5_display.setCursor(22, 31);                            // start at x=3 y=31, with 8 pixel of spacing
  p5_display.print( "M" );

  p5_display.setCursor(44, 31);                            // start at x=38 y=31, with 8 pixel of spacing
  p5_display.print( "I" );
}

// NTP_CLOCK FUNCTION
void ntp_clock()
{
  // sync time now
  loc_time = my_tz.now();

  // call ntp_status function
  ntp_status();

    // display clock separator ":" with ntp_status color
    p5_display.setTextSize(2);                             // size 2 == 10 pixels high 8 pixels wide
    p5_display.setTextColor(sync_result);
    p5_display.fillRect(21, 8, 2, 10, p5_BLACK);
    p5_display.setCursor(21, 18);                          // start at x=21 y=23, with 8 pixel of spacing
    p5_display.print( ":" );
    p5_display.fillRect(41, 8, 2, 10, p5_BLACK);
    p5_display.setCursor(41, 18);                          // start at x=41 y=23, with 8 pixel of spacing
    p5_display.print( ":" );

  // display time - second (SS)
  p5_display.setTextSize(2);                               // size 2 == 10 pixels high 8 pixels wide
  p5_display.setTextColor(p5_GREY);
  p5_display.fillRect(45, 8, 16, 10, p5_BLACK);
  p5_display.setCursor(45, 18);                            // start at x=45 y=18, with 8 pixel of spacing
  p5_display.printf( "%02u", second(loc_time) );

  // display time - minute (MM)
  if ( prev_minute != minute(loc_time) )
  {
    prev_minute = minute(loc_time);
    p5_display.fillRect(25, 8, 16, 10, p5_BLACK);
    p5_display.setCursor(25, 18);                          // start at x=25 y=18, with 8 pixel of spacing
    p5_display.printf( "%02u", minute(loc_time) );
  }

  // display time - hour (HH)
  if ( prev_hour != hour(loc_time) )
  {
    prev_hour = hour(loc_time);
    p5_display.fillRect(5, 8, 16, 10, p5_BLACK);
    p5_display.setCursor(5, 18);                           // start at x=5 y=18, with 8 pixel of spacing
    p5_display.printf( "%02u", hour(loc_time) );
  }

  if ( prev_loc_dow != weekday(loc_time) )
  {
  // call jadwal_sholat function every day
    jadwal_sholat();

  // display day of the week (dow)
    prev_loc_dow = weekday(loc_time);
    p5_display.setTextSize(1);                             // size 1 == 8 pixels high 6 pixels wide
    p5_display.setTextColor(p5_BLUE);
    p5_display.fillRect(1, 1, 24, 6, p5_BLACK);
    p5_display.setCursor(1, 6);                            // start at x=39 y=6
    // p5_display.setCursor(dow_x_pos[prev_loc_dow-1], 6); // start at top left, with 8 pixel of spacing
    p5_display.println(dow_matrix[prev_loc_dow-1]);

  // display date (DD/MM/YY)
    p5_display.setTextSize(1);                             // size 1 == 8 pixels high 6 pixels wide
    p5_display.setTextColor(p5_DMCYAN);
    p5_display.fillRect(32, 1, 30, 6, p5_BLACK);
    p5_display.setCursor(32, 6);                           // start at x=2 y=7, with 8 pixel of spacing
    p5_display.printf( "%02u/%02u/%02u", day(loc_time), month(loc_time), (year(loc_time) - 2000) );
    // p5_display.printf( "%02u/%02u", day(loc_time), month(loc_time) );

  // display waktu sholat
    p5_display.setTextSize(1);                             // size 1 == 8 pixels high 6 pixels wide
    p5_display.setTextColor(p5_DMMAGENTA);
    p5_display.fillRect(4, 20, 17, 5, p5_BLACK);
    p5_display.setCursor(4, 25);                           // start at x=4 y=25, with 8 pixel of spacing
    p5_display.print( b_subuh );

    p5_display.fillRect(26, 20, 17, 5, p5_BLACK);
    p5_display.setCursor(26, 25);                          // start at x=26 y=25, with 8 pixel of spacing
    p5_display.print( b_terbit );

    p5_display.fillRect(48, 20, 17, 5, p5_BLACK);
    p5_display.setCursor(48, 25);                          // start at x=48 y=25, with 8 pixel of spacing
    p5_display.print( b_dzuhur );

    p5_display.fillRect(5, 26, 17, 5, p5_BLACK);
    p5_display.setCursor(5, 31);                           // start at x=5 y=31, with 8 pixel of spacing
    p5_display.print( b_ashar );

    p5_display.fillRect(27, 26, 17, 5, p5_BLACK);
    p5_display.setCursor(27, 31);                          // start at x=27 y=31, with 8 pixel of spacing
    p5_display.print( b_maghrib );

    p5_display.fillRect(48, 26, 17, 5, p5_BLACK);
    p5_display.setCursor(48, 31);                          // start at x=48 y=31, with 8 pixel of spacing
    p5_display.print( b_isya );

  }
}

// SCROLL_TEXT FUNCTION (y, delay, text, red color, green color, blue color)
void scroll_text(uint8_t y_pos, unsigned long scroll_delay, String text, uint8_t R_color, uint8_t G_color, uint8_t B_color)
{
  uint16_t text_length = text.length() + 12;
  p5_display.setTextWrap(false);                           // we don't wrap text so it scrolls nicely
  p5_display.setFont(NULL);
  p5_display.setTextSize(1);
  p5_display.setRotation(0);
  p5_display.setTextColor(p5_display.color565(R_color, G_color, B_color));

  // Asuming 5 pixel average character width
  for (int x_pos=p5_width; x_pos>-(p5_width+text_length*5); x_pos--)
  {
    p5_display.setTextColor(p5_display.color565(R_color, G_color, B_color));
//      p5_display.clearDisplay();
    p5_display.fillRect(0, y_pos, p5_width, 8, p5_BLACK);
    p5_display.setCursor(x_pos,y_pos);
    p5_display.println(text);
    delay(scroll_delay);
    yield();
  }
}

// JADWAL_SHOLAT FUNCTION
void jadwal_sholat()
{
  WiFiClientSecure client;
  client.setInsecure();                                    // use with caution
  client.connect("api.myquran.com", 80);

  // url request construct
  HTTPClient https;
  String url = "https://api.myquran.com/v2/sholat/jadwal/";
  url = url + id_kota + "/" + year(loc_time) + "/" + month(loc_time) + "/" + day(loc_time);

  // requesting the table
  https.begin(client, url);
  int httpCode = https.GET();
  String payload = https.getString();
  // Serial.println(url);
  // Serial.print(payload);
  // Serial.print("\r\n\r\n");

  // deserialize JSON payload
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  JsonObject results = doc["data"]["jadwal"];
  String imsak = results["imsak"];
  String subuh = results["subuh"];
  String terbit = results["terbit"];
  String dhuha = results["dhuha"];
  String dzuhur = results["dzuhur"];
  String ashar = results["ashar"];
  String maghrib = results["maghrib"];
  String isya = results["isya"];

  // convert string to character
  imsak.toCharArray(b_imsak, 10);
  subuh.toCharArray(b_subuh, 10);
  terbit.toCharArray(b_terbit, 10);
  dhuha.toCharArray(b_dhuha, 10);
  dzuhur.toCharArray(b_dzuhur, 10);
  ashar.toCharArray(b_ashar, 10);
  maghrib.toCharArray(b_maghrib, 10);
  isya.toCharArray(b_isya, 10);

  // on JSON deserialization error
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
}

// NTP_STATUS FUNCTION - inspired by W8BH - Bruce E. Hall - https://github.com/bhall66/NTP-clock
void ntp_status()
{
  if (second() % 30) return;                               // update every 30 seconds
    sync_age = now() - lastNtpUpdateTime();                // how long since last sync?
  if (sync_age < SYNC_MARGINAL)                            // time is good & in sync
    sync_result = p5_DMGREEN;
  else if (sync_age < SYNC_LOST)                           // sync is 1-24 hours old
    sync_result = p5_DMYELLOW;
  else sync_result = p5_DMRED;                             // time is stale!
}

// SETUP
void setup()
{
  // initializing Serial Port
  Serial.begin(115200);                                    // set serial port speed
  delay(1000);                                             // 3 seconds delay
  Serial.println();

  p5_display.begin(16);                                    // Matrix Panel scan ratio (our p5 is a 1/16)

  // Define multiplex implemention here {BINARY, STRAIGHT} (default is BINARY)
    // p5_display.setMuxPattern(BINARY);
  // Set the multiplex pattern {LINE, ZIGZAG,ZZAGG, ZAGGIZ, WZAGZIG, VZAG, ZAGZIG} (default is LINE)
    // p5_display.setScanPattern(LINE);
  // Rotate display
    // p5_display.setRotate(true);
  // Flip display
    // p5_display.setFlip(true);
  // Control the minimum color values that result in an active pixel
    // p5_display.setColorOffset(5, 5, 5);
  // Set the multiplex implemention {BINARY, STRAIGHT} (default is BINARY)
    // p5_display.setMuxPattern(BINARY);
  // Set the color order {RRGGBB, RRBBGG, GGRRBB, GGBBRR, BBRRGG, BBGGRR} (default is RRGGBB)
    // p5_display.setColorOrder(GGRRBB);
  // Set the time in microseconds that we pause after selecting each mux channel
  // (May help if some rows are missing / the mux chip is too slow)
    p5_display.setMuxDelay(1,1,1,1,1);
  // Set the number of panels that make up the display area width (default is 1)
    // p5_display.setPanelsWidth(2);
  // Set the brightness of the panels (default is 255)
     p5_display.setBrightness(brightness);
  // Set driver chip type (default is FM6124)
    // p5_display.setDriverChip(FM6124);

  p5_display.clearDisplay();
  p5_display.showBuffer();
  display_update_enable(true);

  // WiFiManager, Local initialization. 
  // Once its business is done, there is no need to keep it around
  WiFiManager wfm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
//    wfm.resetSettings();

    Serial.println("WiFi connecting");

    if (!wfm.autoConnect( "P5-NTP-JS" ))
    {
      // Did not connect, print error message
      Serial.println("failed to connect and hit timeout");
   
      // Reset and try again
      ESP.restart();
      delay(1000);
    }
 
    // Connected!
    Serial.println( "WiFi Connected" );
    Serial.print( "IP Address: " );
    Serial.println( WiFi.localIP() );
    Serial.println( sketchName );
    p5_display.setFont( &TomThumb );
    p5_display.setTextColor(p5_GREY);
    p5_display.println( "WiFi Connected" );
    p5_display.println( "IP Address:" );
    p5_display.println( WiFi.localIP() );
    p5_display.println( sketchName );
    delay(5000);
    p5_display.clearDisplay();

  // set hostname
  WiFi.setHostname(new_hostname.c_str());

  // priming eztime library
  setServer(ntp_pool);                                     // set NTP server
  while (timeStatus()!=timeSet)                            // wait until ntp synced
  {
    events();                                              // allow ezTime to sync
    delay(100);
  }
  my_tz.setLocation(t_zone);

  // display scrolling text
  scroll_text(12, 60, "This is NTP based Salat Time on P5 Display Module", 32, 128, 128);

  // call static_display function
  static_display();
}

// MAIN LOOP
void loop()
{
  if (secondChanged()) ntp_clock();
  events();                                                // update NTP
  delay(100);
}

// PROGRAM END
