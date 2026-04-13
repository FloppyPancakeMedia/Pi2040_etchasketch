#include <SdFat.h>

#define SD_FAT_TYPE 1  // FAT16/32
#define SPI_SPEED SD_SCK_MHZ(12)

#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else
#error Invalid SD_FAT_TYPE
#endif

SPIClassRP2040 spi1bus(spi1, SD_MISO, SD_CS, SD_CLK, SD_MOSI);
char curFile[32];

void setupSD() {
  if (!sd.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(12), &spi1bus))) {
    Serial.println("Failed to init SD card");
    if (sd.card()->errorCode()) {
      Serial.printf("Error Code: %s\n", sd.card()->errorCode());
      Serial.printf("Error Data: %s\n", sd.card()->errorData());
    }
    if (sd.vol()->fatType() == 0) {
      Serial.printf("Can't find valid FAT16/32/exFAT partition\n");
    }
  } else {
    /* GET SD CARD INFO */
    uint32_t size = sd.card()->sectorCount();
    if (size == 0) {
      Serial.println("Can't determine the card size");
      return;
    }
    uint32_t sizeMB = 0.000512 * size + 0.5;
    Serial.printf("Card size: %dMB\n", sizeMB);
    if (sd.fatType() <= 32) {
      Serial.printf("Volume is FAT: %d\n", sd.fatType());
    } else {
      Serial.println("Volume is exFAT");
    }
    Serial.printf("Cluster size (bytes): %d\n\n", sd.vol()->bytesPerCluster());
    Serial.printf("Files found (date time size name): \n");
    sd.ls(LS_R | LS_DATE | LS_SIZE);
    Serial.println();
    if ((sizeMB > 1100 && sd.vol()->sectorsPerCluster() < 64) || (sizeMB < 2200 && sd.vol()->fatType() == 32)) {
      Serial.println("This card should be reformatted for best performance.");
      Serial.println("Use a cluster size of 32 KB for cards larger than 1 GB.");
      Serial.println("Only cards larger than 2 GB should be formatted FAT32");
    }
    /* END GET INFO */

    // Create dir if not exists
    if (!sd.chdir("sketches")){
      sd.mkdir("sketches");
      if (sd.chdir("sketches") == 0){
        Serial.println("Something went seriously wrong in changing directories.");
      }
    }
    Serial.println("Changed directory");
    sd.ls(LS_R | LS_DATE | LS_SIZE);
  }
}

#define BIT_DEPTH 16
#define PIXEL_DATA_OFFSET 66
#define ROW_SIZE (DIMENSIONS * 2)
#define PIXEL_DATA_SIZE (ROW_SIZE * DIMENSIONS)
#define FILE_SIZE (PIXEL_DATA_OFFSET * PIXEL_DATA_SIZE)

void save() {
  Serial.printf("Preparing to save:\tls\n");
  sd.ls(LS_R | LS_SIZE);

  if (strcmp(curFile, "") == 0) {
    // Not working on saved file
    sprintf(curFile, "/sketches/%d-%d.bmp", curX, curY);
    uint8_t count = 1;
    while(sd.exists(curFile)){
      sprintf(curFile, "/sketches/%d-%d_%d", curX, curY, count++);
    }
  }
  tft.setCursor(DIMENSIONS / 2 - 40, DIMENSIONS / 2);
  tft.print("Saving...");

  Serial.printf("Opening file %s\n", curFile);
  file = sd.open(curFile, O_CREAT | O_WRITE);
  if (!file){
    Serial.printf("Error opening file %s\n", curFile);
    Serial.printf("Error code: %x\n", sd.sdErrorCode());
    Serial.printf("Error data: %x\n", sd.sdErrorData());
    return;
  }
  Serial.println("Writing header...");
  /* ###### AI WARNING: HEADER ####### */
  // FILE HEADER (14 bytes)
  uint8_t header[66] = {0};
  uint8_t *h = header;
  h[0] = 'B'; h[1] = 'M';   // Magic number
  *(uint32_t *)(h + 2) = FILE_SIZE;
  *(uint32_t *)(h + 6) = 0;
  *(uint32_t *)(h + 10) = PIXEL_DATA_OFFSET;

  // DIB HEADER (40 bytes)
  *(uint32_t *)(h + 14) = 40;                       // DIB header size
  *(int32_t  *)(h + 18) = DIMENSIONS;               // Width in pixels
  *(int32_t  *)(h + 22) = -DIMENSIONS;              // Negative = top-down row order
  *(uint16_t *)(h + 26) = 1;                        // Color planes (always 1)
  *(uint16_t *)(h + 28) = BIT_DEPTH;                // Bits per pixel
  *(uint32_t *)(h + 30) = 3;                        // Compression: BI_BITFIELDS
  *(uint32_t *)(h + 34) = PIXEL_DATA_SIZE;          // Pixel data size
  *(int32_t  *)(h + 38) = 2835;                     // X pixels per meter (~72 DPI)
  *(int32_t  *)(h + 42) = 2835;                     // Y pixels per meter (~72 DPI)
  *(uint32_t *)(h + 46) = 0;                        // Colors in table (0 = default)
  *(uint32_t *)(h + 50) = 0;                        // Important colors (0 = all)

  // RGB565 Color Masks (12 bytes)
  *(uint32_t *)(h + 54) = 0xF800;                   // Red mask (bits 15-11)
  *(uint32_t *)(h + 58) = 0x07E0;                   // Green mask (bits 10-5)
  *(uint32_t *)(h + 62) = 0x001F;                   // Blue mask (bits 4-0)
  /* ###### END AI WARNING: HEADER COMPLETE ######*/

  Serial.println("Writing data...");
  file.write(header, sizeof(header));
  file.write(bitmap, sizeof(bitmap));
  file.close();

  Serial.println("Resetting display");
  tft.setCursor(DIMENSIONS / 2 - 40, DIMENSIONS / 2);
  tft.setTextColor(ST77XX_BLACK);
  tft.print("Saving...");
  tft.setCursor(DIMENSIONS / 2 - 50, DIMENSIONS / 2);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Resetting...");
  repaintFromBitmap(0, 0, DIMENSIONS, DIMENSIONS, true);
}