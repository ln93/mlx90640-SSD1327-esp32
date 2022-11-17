//
// Sample sketch to demo the ssd1327 library on a 256x64x4-bpp
// SSD1322 display. The library currently supports 2 display types:
// SSD1322 (256x64x4-bpp) and SSD1327 (128x128x4-bpp)
// For my testing, I connected the display to a Lolin D32
// ESP32 board and used the following pins to communicate
// with the display. Change these to fit your setup
//
#define DC_PIN 12
#define CS_PIN 5
#define RESET_PIN 13

#include "ssd1327.h"

#include <Wire.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
// 40x40 pattern bitmap
// Classic Mac bomb icon
// This is a 1-bpp image that will be drawn using
// the ssd1327DrawPattern() function. This function
// draws a transparent pattern in any 'color'. Each 1 bit
// will be drawn in the given color and 0 bits will be transparent
float tmin = 0, tmax = 0;
void drawcross(int x, int y)
{

  ssd1327SetPixel(x, y - 2, 0);
  ssd1327SetPixel(x, y + 2, 0);
  ssd1327SetPixel(x - 2, y, 0);
  ssd1327SetPixel(x + 2, y, 0);
  ssd1327SetPixel(x, y - 1, 0);
  ssd1327SetPixel(x, y + 1, 0);
  ssd1327SetPixel(x - 1, y, 0);
  ssd1327SetPixel(x + 1, y, 0);
}
void bilinear(float a[], float min, float max, float temp[])
{ // 32*24
  char iradd[32];
  // fix badpoint
  a[32 * 23 + 31] = a[32 * 23 + 30];
  float mi = 999.0, ma = -50.0;
  for (int i = 0; i < 124; i++)
  {
    for (int j = 0; j < 92; j++)
    {
      // sprintf(iradd,"%d,%d",i,j);
      // Serial.println(iradd);

      float tmpx = a[32 * (j / 4) + i / 4] * (4.0 - i % 4) / 4.0 + a[32 * (j / 4) + i / 4 + 1] * (i % 4) / 4.0;
      float tmpx2 = a[32 * (j / 4 + 1) + i / 4] * (4.0 - i % 4) / 4.0 + a[32 * (j / 4 + 1) + i / 4 + 1] * (i % 4) / 4.0;
      float tmp = tmpx * (4.0 - j % 4) / 4.0 + tmpx2 * (j % 4) / 4.0;

      if (tmp < mi)
        mi = tmp;
      if (tmp > ma)
        ma = tmp;

      if (tmp < min)
        tmp = min;
      if (tmp > max)
        tmp = max;

      // sprintf(iradd,"tmp:%f",tmp);
      // Serial.println(iradd);
      ssd1327SetPixel(125 - i, 2 + j, (tmp - min) / (max - min) * 15);
    }
  }
  temp[0] = mi;
  temp[1] = ma;
  temp[2] = a[32 * 12 + 16];
}

void ShowGraphic(float temp[])
{
  uint8_t u8Temp[8 * 40]; // holds the rotated mask
  char szTemp[32];
  char szTemp2[32];
  getimg(temp);
  sprintf(szTemp, "MIN %.1f  MAX %.1f      ", temp[0], temp[1]); // show current angle
  if(temp[1]<38.0&&temp[1]>32.0)
    sprintf(szTemp2, "TGT %.2f SAFE        ", temp[2]);    
  else if(temp[1]>=38.0)
    sprintf(szTemp2, "TGT %.2f !!FEVER!! ", temp[2]);     
  else 
    sprintf(szTemp2, "TGT %.2f NO HUMAN    ", temp[2]);    

  ssd1327WriteString(0, 96, szTemp, FONT_SMALL, 15, 0);
  ssd1327WriteString(0, 112, szTemp2, FONT_SMALL, 15, 0);
  Serial.println(szTemp);
  Serial.println(szTemp2);
  // Rotate the source bitmap to the given angle and store
  drawcross(64, 48);
  ssd1327ShowBitmap(NULL, 0, 0, 0, 128, 128);
} /* ShowGraphic() */

byte MLX90640_address = 0x33; // Default 7-bit unshifted address of the MLX90640

#define TA_SHIFT 8 // Default shift for MLX90640 in open air

static float mlx90640Tosafe[768 + 256];
float *mlx90640To = &(mlx90640Tosafe[127]);
paramsMLX90640 mlx90640;

void setup()
{
  Serial.begin(115200);
  ssd1327SPIInit(OLED_128x128, DC_PIN, CS_PIN, RESET_PIN, 1, 0, 36000000);
  Wire.begin(21, 22, 400000);

  char iradd[32];
  while (isConnected() == false)
  {
    sprintf(iradd, "BAD ADD %d", MLX90640_address);
    MLX90640_address += 2;
    ssd1327WriteString(0, 0, iradd, FONT_NORMAL, 15, 0);
    ssd1327ShowBitmap(NULL, 0, 0, 0, 128, 128);
    delay(50);
  }
  sprintf(iradd, "IR ADDR %d", MLX90640_address);
  ssd1327WriteString(0, 0, iradd, FONT_NORMAL, 15, 0);
  ssd1327ShowBitmap(NULL, 0, 0, 0, 128, 128);

  // Get device parameters - We only have to do this once
  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if (status != 0)
    ssd1327WriteString(0, 0, "Failed to load system parameters", FONT_NORMAL, 15, 0);
  else
    ssd1327WriteString(0, 0, "load sys param", FONT_NORMAL, 15, 0);
  ssd1327ShowBitmap(NULL, 0, 0, 0, 128, 128);

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0)
    ssd1327WriteString(0, 0, "Parameter extraction failed", FONT_NORMAL, 15, 0);
  else
    ssd1327WriteString(0, 0, "param extracted", FONT_NORMAL, 15, 0);
  ssd1327ShowBitmap(NULL, 0, 0, 0, 128, 128);
  MLX90640_SetRefreshRate(MLX90640_address, 4); // 32hz
  // Once params are extracted, we can release eeMLX90640 array
}

void getimg(float temp[])
{
  for (byte x = 0; x < 2; x++) // Read both subpages
  {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0)
    {
      ssd1327WriteString(0, 0, "Get Frame ERR", FONT_NORMAL, 15, 0);
      ssd1327ShowBitmap(NULL, 0, 0, 0, 128, 128);
    }

    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);

    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

    float tr = Ta - TA_SHIFT; // Reflected temperature based on the sensor ambient temperature
    float emissivity = 0.95;
    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
  }
  bilinear(mlx90640To, 20, 40, temp);
}

// Returns true if the MLX90640 is detected on the I2C bus
boolean isConnected()
{
  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0)
    return (false); // Sensor did not ACK
  return (true);
}

void loop()
{
  float temp[3] = {10.0,40.0,0.0};
  ssd1327Fill(0);
  // Rotate a small bitmap
  while (1)
  {
    ShowGraphic(temp);
    delay(16);
  }

} // loop
