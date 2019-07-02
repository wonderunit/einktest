#define ENABLE_GxEPD2_GFX 1

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

#include <WiFi.h>

#include <WiFiClient.h>
#include <WiFiClientSecure.h>


#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include <Fonts/FreeSans18pt7b.h>

#include <Fonts/FreeSansBold24pt7b.h>

#include <Open_Sans_ExtraBold_100Bitmaps.h>
#include <Open_Sans_ExtraBold_150.h>

#include "qrcode.h"



GxEPD2_BW<GxEPD2_it60, GxEPD2_it60::HEIGHT / 8> display(GxEPD2_it60(/*CS=5*/ 5, /*DC=*/0, /*RST=*/16, /*BUSY=*/4));

const char *ssid = "pupkit";
const char *password = "aabbaabbaa";

const int httpPort = 80;
const int httpsPort = 443;

const char *fp_rawcontent = "cc aa 48 48 66 46 0e 91 53 2c 9c 7c 23 2a b1 74 4d 29 9d 33";

const char *host_rawcontent = "raw.githubusercontent.com";
const char *path_rawcontent = "/ZinggJM/GxEPD2/master/extras/bitmaps/";
const char *path_prenticedavid = "/prenticedavid/MCUFRIEND_kbv/master/extras/bitmaps/";

void showBitmapFrom_HTTPS(const char *host, const char *path, const char *filename, const char *fingerprint, int16_t x, int16_t y, bool with_color = true);

uint16_t read16(WiFiClient &client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  return result;
}

uint32_t read32(WiFiClient &client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read();
  ((uint8_t *)&result)[2] = client.read();
  ((uint8_t *)&result)[3] = client.read(); // MSB
  return result;
}
uint32_t skip(WiFiClient &client, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      int16_t v = client.read();
      remain--;
    }
    else
      delay(1);
    if (millis() - start > 2000)
      break; // don't hang forever
  }
  return bytes - remain;
}

uint32_t read(WiFiClient &client, uint8_t *buffer, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      int16_t v = client.read();
      *buffer++ = uint8_t(v);
      remain--;
    }
    else
      delay(1);
    if (millis() - start > 2000)
      break; // don't hang forever
  }
  return bytes - remain;
}

static const uint16_t input_buffer_pixels = 800; // may affect performance

static const uint16_t max_row_width = 800;      // for up to 7.5" display
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels];        // up to depth 24
uint8_t output_row_mono_buffer[max_row_width * 50];   // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8];   // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8];  // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w

void showBitmapFrom_HTTPS(const char *host, const char *path, const char *filename, const char *fingerprint, int16_t x, int16_t y, bool with_color) {
  WiFiClientSecure client;
  bool connection_ok = false;
  bool valid = false; // valid format to be handled
  bool flip = true;   // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  if ((x >= display.width()) || (y >= display.height()))
    return;
  Serial.println();
  Serial.print("downloading file \"");
  Serial.print(filename);
  Serial.println("\"");
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort))
  {
    Serial.println("connection failed");
    return;
  }
  Serial.print("requesting URL: ");
  Serial.println(String("https://") + host + path + filename);
  client.print(String("GET ") + path + filename + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: GxEPD2_WiFi_Example\r\n" +
               "Connection: close\r\n\r\n");
  Serial.println("request sent");
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (!connection_ok)
    {
      connection_ok = line.startsWith("HTTP/1.1 200 OK");
      if (connection_ok)
        Serial.println(line);
      //if (!connection_ok) Serial.println(line);
    }
    if (!connection_ok)
      Serial.println(line);
    //Serial.println(line);
    if (line == "\r")
    {
      Serial.println("headers received");
      break;
    }
  }
  if (!connection_ok)
    return;
  // Parse BMP header
  if (read16(client) == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(client);
    uint32_t creatorBytes = read32(client);
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    uint32_t width = read32(client);
    uint32_t height = read32(client);
    uint16_t planes = read16(client);
    uint16_t depth = read16(client); // bits per pixel
    uint32_t format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2;                   // read so far
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      Serial.print("File size: ");
      Serial.println(fileSize);
      Serial.print("Image Offset: ");
      Serial.println(imageOffset);
      Serial.print("Header size: ");
      Serial.println(headerSize);
      Serial.print("Bit Depth: ");
      Serial.println(depth);
      Serial.print("Image size: ");
      Serial.print(width);
      Serial.print('x');
      Serial.println(height);
      // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (depth < 8)
        rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= display.width())
        w = display.width() - x;
      if ((y + h - 1) >= display.height())
        h = display.height() - y;
      if (w <= max_row_width) // handle with direct drawing
      {
        valid = true;
        uint8_t bitmask = 0xFF;
        uint8_t bitshift = 8 - depth;
        uint16_t grayscale;
        uint16_t red;
        uint16_t green;
        uint16_t blue;

        display.setPartialWindow(x, y, w, h);

        //display.clearScreen();
        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
        //Serial.print("skip "); Serial.println(rowPosition - bytes_read);
        uint32_t out_idx = 0;
        uint32_t rowBufferIndex = 0;
        bytes_read += skip(client, rowPosition - bytes_read);
        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
        {
          if (!connection_ok || !(client.connected() || client.available()))
            break;
          delay(1); // yield() to avoid WDT
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0;           // for depth <= 8
          uint8_t in_bits = 0;           // for depth <= 8
          uint8_t out_byte = 0xFF;       // white (for w%8!=0 boarder)
          uint8_t out_color_byte = 0xFF; // white (for w%8!=0 boarder)

          for (uint16_t col = 0; col < w; col++) // for each pixel
          {
            //yield();
            if (!connection_ok || !(client.connected() || client.available()))
              break;
            // Time to read more pixel data?
            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
            {
              uint32_t get = in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain;
              uint32_t got = read(client, input_buffer, get);
              while ((got < get) && connection_ok)
              {
                //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.println(bytes_read);
                //if ((get - got) > client.available()) delay(200); // does improve? yes, if >= 200
                uint32_t gotmore = read(client, input_buffer + got, get - got);
                got += gotmore;
                connection_ok = gotmore > 0;
              }
              in_bytes = got;
              in_remain -= got;
              bytes_read += got;
            }
            if (!connection_ok)
            {
              Serial.print("Error: got no more after ");
              Serial.print(bytes_read);
              Serial.println(" bytes read!");
              break;
            }
            switch (depth)
            {
            case 24:
              blue = input_buffer[in_idx++];
              green = input_buffer[in_idx++];
              red = input_buffer[in_idx++];
              break;
            case 16:
            {
              uint8_t lsb = input_buffer[in_idx++];
              uint8_t msb = input_buffer[in_idx++];
              if (format == 0) // 555
              {
                blue = (lsb & 0x1F) << 3;
                green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                red = (msb & 0x7C) << 1;
              }
              else // 565
              {
                blue = (lsb & 0x1F) << 3;
                green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                red = (msb & 0xF8);
              }
            }
              output_row_mono_buffer[out_idx++] = blue;
            break;
            case 1:
            case 4:
            case 8:
            {
              if (0 == in_bits)
              {
                in_byte = input_buffer[in_idx++];
                in_bits = 8;
                blue = in_byte;
                //Serial.println(blue);
                //output_row_mono_buffer[out_idx++] = blue;
              }
              uint16_t pn = (in_byte >> bitshift) & bitmask;

              //Serial.println(pn*16);
              output_row_mono_buffer[out_idx++] = pn*16;
              //Serial.println(pn);
              blue = pn / 16;
              in_byte <<= depth;
              in_bits -= depth;
            }
            break;
            }

          } // end pixel
          int16_t yrow = y + (flip ? h - row - 1 : row);
          //display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
          //  display.writeNative(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1, false, false, false);
          rowBufferIndex++;

          if (rowBufferIndex == 49) {
            display.writeNative(output_row_mono_buffer, 0, x, yrow, w, 50, false, true, true);
            rowBufferIndex = 0;
            out_idx = 0;
          }


          if (yrow == 0) {
            display.writeNative(output_row_mono_buffer, 0, x, yrow, w, rowBufferIndex, false, true, true);
          }
        } // end line



        Serial.print("downloaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
        Serial.print("bytes read ");
        Serial.println(bytes_read);
        display.refresh();
        display.setFullWindow();
      }
    }
  }
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }
}

// void drawTest() {
//         for (uint16_t z = 0; z < 255; z++) // for each pixel
//           {
//           output_row_mono_buffer[z] = z;
//           }

//   display.firstPage();
//   do
//   {
//     display.fillScreen(GxEPD_WHITE); // set the background to white (fill the buffer with value for white)
//     display.drawRect(40, 40, 100, 100, 0x00);
//   } while (display.nextPage());

//   delay(5000);

//   display.drawNative(output_row_mono_buffer, 0, 0, 0, 255, 1, false, false, true);
//   delay(5000);

//   display.setPartialWindow(0, 10, display.width(), 100);
//   display.firstPage();
//   do
//   {
//     display.drawLine(0,0, 100,100,0);
//     display.drawRect(20, 20, 100, 100, 0);
//   } while (display.nextPage());

//  delay(5000);

// }

void drawBitmaps_other()
{
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "test5.bmp", fp_rawcontent, 0, 0);
  delay(3000);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "test3.bmp", fp_rawcontent, 0, 0);
  delay(3000);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "test4.bmp?1", fp_rawcontent, 0, 0);
  delay(3000);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "test800x4.bmp", fp_rawcontent, 0, 0);
  delay(3000);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "test6.bmp", fp_rawcontent, 0, 0);
}

void drawBitmapsStoryboard()
{
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "teststoryboard.bmp", fp_rawcontent, 0, 0);
}

void drawQRCode()
{

  QRCode qrcode;
  uint8_t qrcodeBytes[qrcode_getBufferSize(3)];

  qrcode_initText(&qrcode, qrcodeBytes, 2, ECC_LOW, "134/189/4/24:24:24:24");
  Serial.print(qrcode.size);

  uint16_t chunkSize = 600 / (qrcode.size + 2);

  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE); // set the background to white (fill the buffer with value for white)

    for (uint16_t y = 0; y < qrcode.size; y++)
    {
      for (uint16_t x = 0; x < qrcode.size; x++)
      {
        if (qrcode_getModule(&qrcode, x, y))
        {
          display.fillRect((x + 1) * chunkSize, (y + 1) * chunkSize, chunkSize, chunkSize, 0x00);
        }
        else
        {
          Serial.print("  ");
        }
      }
      Serial.print("\n");
    }

    display.setFont(&FreeSans9pt7b);
    display.setCursor(600, 100-30);
    display.print("SCN");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(600-10, 100-30+75);
    display.print("SHOT");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(600-10, 100-30+75+75);
    display.print("TAKE");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(600-10, 350-30);
    display.print("TIME");

    uint16_t xOffset = 650;

    display.setFont(&Open_Sans_ExtraBold_100);
    display.setCursor(xOffset, 100);
    display.print("3");
    display.setCursor(xOffset, 100+75);
    display.println("4");
    display.setCursor(xOffset, 100+75+75);
    display.println("8");

    display.setFont(&Open_Sans_ExtraBold_100);
    display.setCursor(xOffset, 350);
    display.print("03");
    display.setCursor(xOffset, 350+75);
    display.println("23");
    display.setCursor(xOffset, 350+75+75);
    display.println("14");
    display.setCursor(xOffset, 350+75+75+75);
    display.println("12");


  } while (display.nextPage());
  delay(50000);
}

void showShot() {

  display.setTextColor(0);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeSans9pt7b);
    display.setCursor(100, 20);
    display.print("SCENE");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(400, 20);
    display.print("SHOT");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(650, 20);
    display.print("TAKE");

    display.setFont(&Open_Sans_ExtraBold_150);
    display.setCursor(100, 150);
    display.print("3");

    display.setFont(&Open_Sans_ExtraBold_150);
    display.setCursor(350, 150);
    display.print("4");

    display.setFont(&Open_Sans_ExtraBold_150);
    display.setCursor(600, 150);
    display.print("8");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(100+100, 150);
    display.print("/ 79");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(350+100, 150);
    display.print("/ 89");


    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(10, 200);
    display.print("3. INT. CAVE LAKE STATUE");
    display.setFont(&FreeSans9pt7b);
    display.setCursor(10, 200+20);
    display.print("The dark tunnel they're sprinting through...");



    //display.drawRect(x, y - tbh, tbw, tbh, GxEPD_BLACK);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(420, 400+50);
    display.print("RYAN: Whoa...");
    display.setFont(&FreeSans9pt7b);
    display.setCursor(420, 400+20+50);
    display.print("The dark tunnel they're sprinting through...");

    display.setFont(&FreeSans18pt7b);
    display.setCursor(420, 500+20);
    display.print("TRI 22mm 1m f/5.6");
    display.setCursor(420, 500+40+20);
    display.print("0:06 -3 0 2m");


    display.setFont(&FreeSans18pt7b);
    display.setCursor(10, 600-15);
    display.print("3:25PM");

    display.setFont(&FreeSans9pt7b);
    display.setCursor(150, 600-15);
    display.print("3/24/2019");





  } while (display.nextPage());

  delay(100);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "next.bmp", fp_rawcontent, 420+174, 300-74-4-40);

  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "camera-plot300.bmp", fp_rawcontent, 10, 240);
  delay(100);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "test4.bmp", fp_rawcontent, 420, 300-40);
  delay(100);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "previous.bmp", fp_rawcontent, 420, 300-74-4-40);
  delay(100);
  showBitmapFrom_HTTPS("raw.githubusercontent.com", "/wonderunit/einktest/master/", "next.bmp", fp_rawcontent, 420+174, 300-74-4-40);


  delay(50000);

}


void setup()
{
  Serial.begin(115200);
  Serial.println("GxEPD2_WiFi_Example");
  Serial.println();
  Serial.println("GxEPD2_WiFi_Example");
  delay(1);
  display.init();

#ifdef RE_INIT_NEEDED
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect();
#endif

  Serial.println("GxEPD2_WiFi_Example");

  delay(1);


  if (!WiFi.getAutoConnect() || (WiFi.getMode() != WIFI_STA) || ((WiFi.SSID() != ssid) && String(ssid) != "........"))
  {
    Serial.println();
    Serial.print("WiFi.getAutoConnect()=");
    Serial.println(WiFi.getAutoConnect());
    Serial.print("WiFi.SSID()=");
    Serial.println(WiFi.SSID());
    WiFi.mode(WIFI_STA); // switch off AP
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
  }
  int ConnectTimeout = 30; // 15 seconds
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    Serial.print(WiFi.status());
    if (--ConnectTimeout <= 0)
    {
      Serial.println();
      Serial.println("WiFi connect timeout");
      return;
    }
  }
  Serial.println();
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.println(WiFi.localIP());


 showShot();

  drawQRCode();


  // drawBitmapsBuffered_200x200();
  // drawBitmapsBuffered_other();
  //drawBitmaps_200x200();
  drawBitmaps_other();

  display.powerOff();

  // drawBitmapsStoryboard();

  //drawBitmaps_test();
  //drawBitmapsBuffered_test();

  Serial.println("GxEPD2_WiFi_Example done");
}

void loop(void)
{
}
