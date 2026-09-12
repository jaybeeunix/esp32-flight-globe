#include "Display_ST7701.h"

spi_device_handle_t SPI_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
static SemaphoreHandle_t s_sem_vsync_end = NULL;
static SemaphoreHandle_t s_sem_gui_ready = NULL;
void ST7701_WriteCommand(uint8_t cmd)
{
  spi_transaction_t spi_tran = {
    .cmd = 0,
    .addr = cmd,
    .length = 0,
    .rxlength = 0,
  };
  spi_device_transmit(SPI_handle, &spi_tran);
}
void ST7701_WriteData(uint8_t data)
{
  spi_transaction_t spi_tran = {
    .cmd = 1,
    .addr = data,
    .length = 0,
    .rxlength = 0,
  };
  spi_device_transmit(SPI_handle, &spi_tran);
}

void ST7701_CS_EN(){
  Set_EXIO(EXIO_PIN3,Low);
  vTaskDelay(pdMS_TO_TICKS(10));
}
void ST7701_CS_Dis(){
  Set_EXIO(EXIO_PIN3,High);
  vTaskDelay(pdMS_TO_TICKS(10));
}
void ST7701_Reset(){
  Set_EXIO(EXIO_PIN1,Low);
  vTaskDelay(pdMS_TO_TICKS(10));
  Set_EXIO(EXIO_PIN1,High);
  vTaskDelay(pdMS_TO_TICKS(50));
}
bool example_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
  (void)panel;
  (void)edata;
  (void)user_ctx;
  BaseType_t high_task_awoken = pdFALSE;
  if (s_sem_gui_ready && s_sem_vsync_end &&
      xSemaphoreTakeFromISR(s_sem_gui_ready, &high_task_awoken) == pdTRUE) {
    xSemaphoreGiveFromISR(s_sem_vsync_end, &high_task_awoken);
  }
  return high_task_awoken == pdTRUE;
}

void ST7701_Init()
{
  // 初始化SPI总线
  spi_bus_config_t buscfg = {
    .mosi_io_num = LCD_MOSI_PIN,
    .miso_io_num = -1,
    .sclk_io_num = LCD_CLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 64, // ESP32 S3 max size is 64Kbytes
  };
  spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  spi_device_interface_config_t devcfg = {
    .command_bits = 1,
    .address_bits = 8,
    .mode = SPI_MODE0,
    .clock_speed_hz = 40000000,
    .spics_io_num = -1,
    .queue_size = 1,            // Not using queues
  };
  spi_bus_add_device(SPI2_HOST, &devcfg, &SPI_handle);

  ST7701_CS_EN();

  // 2.8inch
  ST7701_WriteCommand(0xFF);
  ST7701_WriteData(0x77);
  ST7701_WriteData(0x01);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x13);

  ST7701_WriteCommand(0xEF);
  ST7701_WriteData(0x08);

  ST7701_WriteCommand(0xFF);
  ST7701_WriteData(0x77);
  ST7701_WriteData(0x01);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x10);

  ST7701_WriteCommand(0xC0);
  ST7701_WriteData(0x3B);
  ST7701_WriteData(0x00);

  ST7701_WriteCommand(0xC1);
  ST7701_WriteData(0x10);
  ST7701_WriteData(0x0C);

  ST7701_WriteCommand(0xC2);
  ST7701_WriteData(0x07);
  ST7701_WriteData(0x0A);

  ST7701_WriteCommand(0xC7);
  ST7701_WriteData(0x00);

  ST7701_WriteCommand(0xCC);
  ST7701_WriteData(0x10);

  ST7701_WriteCommand(0xCD);
  ST7701_WriteData(0x08);

  ST7701_WriteCommand(0xB0);
  ST7701_WriteData(0x05);
  ST7701_WriteData(0x12);
  ST7701_WriteData(0x98);
  ST7701_WriteData(0x0E);
  ST7701_WriteData(0x0F);
  ST7701_WriteData(0x07);
  ST7701_WriteData(0x07);
  ST7701_WriteData(0x09);
  ST7701_WriteData(0x09);
  ST7701_WriteData(0x23);
  ST7701_WriteData(0x05);
  ST7701_WriteData(0x52);
  ST7701_WriteData(0x0F);
  ST7701_WriteData(0x67);
  ST7701_WriteData(0x2C);
  ST7701_WriteData(0x11);

  ST7701_WriteCommand(0xB1);
  ST7701_WriteData(0x0B);
  ST7701_WriteData(0x11);
  ST7701_WriteData(0x97);
  ST7701_WriteData(0x0C);
  ST7701_WriteData(0x12);
  ST7701_WriteData(0x06);
  ST7701_WriteData(0x06);
  ST7701_WriteData(0x08);
  ST7701_WriteData(0x08);
  ST7701_WriteData(0x22);
  ST7701_WriteData(0x03);
  ST7701_WriteData(0x51);
  ST7701_WriteData(0x11);
  ST7701_WriteData(0x66);
  ST7701_WriteData(0x2B);
  ST7701_WriteData(0x0F);

  ST7701_WriteCommand(0xFF);
  ST7701_WriteData(0x77);
  ST7701_WriteData(0x01);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x11);

  ST7701_WriteCommand(0xB0);
  ST7701_WriteData(0x5D);

  ST7701_WriteCommand(0xB1);
  ST7701_WriteData(0x3E);

  ST7701_WriteCommand(0xB2);
  ST7701_WriteData(0x81);

  ST7701_WriteCommand(0xB3);
  ST7701_WriteData(0x80);

  ST7701_WriteCommand(0xB5);
  ST7701_WriteData(0x4E);

  ST7701_WriteCommand(0xB7);
  ST7701_WriteData(0x85);

  ST7701_WriteCommand(0xB8);
  ST7701_WriteData(0x20);

  ST7701_WriteCommand(0xC1);
  ST7701_WriteData(0x78);

  ST7701_WriteCommand(0xC2);
  ST7701_WriteData(0x78);

  ST7701_WriteCommand(0xD0);
  ST7701_WriteData(0x88);

  ST7701_WriteCommand(0xE0);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x02);

  ST7701_WriteCommand(0xE1);
  ST7701_WriteData(0x06);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0x08);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0x05);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0x07);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x33);
  ST7701_WriteData(0x33);

  ST7701_WriteCommand(0xE2);
  ST7701_WriteData(0x11);
  ST7701_WriteData(0x11);
  ST7701_WriteData(0x33);
  ST7701_WriteData(0x33);
  ST7701_WriteData(0xF4);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0xF4);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);

  ST7701_WriteCommand(0xE3);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x11);
  ST7701_WriteData(0x11);

  ST7701_WriteCommand(0xE4);
  ST7701_WriteData(0x44);
  ST7701_WriteData(0x44);

  ST7701_WriteCommand(0xE5);
  ST7701_WriteData(0x0D);
  ST7701_WriteData(0xF5);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);
  ST7701_WriteData(0x0F);
  ST7701_WriteData(0xF7);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);
  ST7701_WriteData(0x09);
  ST7701_WriteData(0xF1);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);
  ST7701_WriteData(0x0B);
  ST7701_WriteData(0xF3);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);

  ST7701_WriteCommand(0xE6);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x11);
  ST7701_WriteData(0x11);

  ST7701_WriteCommand(0xE7);
  ST7701_WriteData(0x44);
  ST7701_WriteData(0x44);

  ST7701_WriteCommand(0xE8);
  ST7701_WriteData(0x0C);
  ST7701_WriteData(0xF4);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);
  ST7701_WriteData(0x0E);
  ST7701_WriteData(0xF6);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);
  ST7701_WriteData(0x08);
  ST7701_WriteData(0xF0);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);
  ST7701_WriteData(0x0A);
  ST7701_WriteData(0xF2);
  ST7701_WriteData(0x30);
  ST7701_WriteData(0xF0);

  ST7701_WriteCommand(0xE9);
  ST7701_WriteData(0x36);
  ST7701_WriteData(0x01);

  ST7701_WriteCommand(0xEB);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x01);
  ST7701_WriteData(0xE4);
  ST7701_WriteData(0xE4);
  ST7701_WriteData(0x44);
  ST7701_WriteData(0x88);
  ST7701_WriteData(0x40);

  ST7701_WriteCommand(0xED);
  ST7701_WriteData(0xFF);
  ST7701_WriteData(0x10);
  ST7701_WriteData(0xAF);
  ST7701_WriteData(0x76);
  ST7701_WriteData(0x54);
  ST7701_WriteData(0x2B);
  ST7701_WriteData(0xCF);
  ST7701_WriteData(0xFF);
  ST7701_WriteData(0xFF);
  ST7701_WriteData(0xFC);
  ST7701_WriteData(0xB2);
  ST7701_WriteData(0x45);
  ST7701_WriteData(0x67);
  ST7701_WriteData(0xFA);
  ST7701_WriteData(0x01);
  ST7701_WriteData(0xFF);

  ST7701_WriteCommand(0xEF);
  ST7701_WriteData(0x08);
  ST7701_WriteData(0x08);
  ST7701_WriteData(0x08);
  ST7701_WriteData(0x45);
  ST7701_WriteData(0x3F);
  ST7701_WriteData(0x54);

  ST7701_WriteCommand(0xFF);
  ST7701_WriteData(0x77);
  ST7701_WriteData(0x01);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);
  ST7701_WriteData(0x00);

  ST7701_WriteCommand(0x11);
  delay(120);                //ms

  ST7701_WriteCommand(0x3A);
  ST7701_WriteData(0x66);       // 0x66  /  0x77

  ST7701_WriteCommand(0x36);
  ST7701_WriteData(0x00);

  ST7701_WriteCommand(0x35);
  ST7701_WriteData(0x00);

  ST7701_WriteCommand(0x29);

  ST7701_CS_Dis();

  // Release SPI2 bus so GPIO1 and GPIO2 can be used by 1-bit SD_MMC
  if (SPI_handle) {
    spi_bus_remove_device(SPI_handle);
    spi_bus_free(SPI2_HOST);
    SPI_handle = NULL;
  }

  //  RGB
  esp_lcd_rgb_panel_config_t rgb_config;
  memset(&rgb_config, 0, sizeof(rgb_config));
  rgb_config.clk_src = LCD_CLK_SRC_PLL160M;
  rgb_config.timings.pclk_hz = ESP_PANEL_LCD_RGB_TIMING_FREQ_HZ;
  rgb_config.timings.h_res = ESP_PANEL_LCD_HEIGHT;
  rgb_config.timings.v_res = ESP_PANEL_LCD_WIDTH;
  rgb_config.timings.hsync_pulse_width = ESP_PANEL_LCD_RGB_TIMING_HPW;
  rgb_config.timings.hsync_back_porch = ESP_PANEL_LCD_RGB_TIMING_HBP;
  rgb_config.timings.hsync_front_porch = ESP_PANEL_LCD_RGB_TIMING_HFP;
  rgb_config.timings.vsync_pulse_width = ESP_PANEL_LCD_RGB_TIMING_VPW;
  rgb_config.timings.vsync_back_porch = ESP_PANEL_LCD_RGB_TIMING_VBP;
  rgb_config.timings.vsync_front_porch = ESP_PANEL_LCD_RGB_TIMING_VFP;
  rgb_config.timings.flags.pclk_active_neg = ESP_PANEL_LCD_RGB_PCLK_ACTIVE_NEG;
  rgb_config.data_width = ESP_PANEL_LCD_RGB_DATA_WIDTH;
  rgb_config.psram_trans_align = 64;
  rgb_config.hsync_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_HSYNC;
  rgb_config.vsync_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_VSYNC;
  rgb_config.de_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_DE;
  rgb_config.pclk_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_PCLK;
  rgb_config.data_gpio_nums[0] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA0;
  rgb_config.data_gpio_nums[1] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA1;
  rgb_config.data_gpio_nums[2] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA2;
  rgb_config.data_gpio_nums[3] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA3;
  rgb_config.data_gpio_nums[4] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA4;
  rgb_config.data_gpio_nums[5] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA5;
  rgb_config.data_gpio_nums[6] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA6;
  rgb_config.data_gpio_nums[7] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA7;
  rgb_config.data_gpio_nums[8] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA8;
  rgb_config.data_gpio_nums[9] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA9;
  rgb_config.data_gpio_nums[10] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA10;
  rgb_config.data_gpio_nums[11] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA11;
  rgb_config.data_gpio_nums[12] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA12;
  rgb_config.data_gpio_nums[13] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA13;
  rgb_config.data_gpio_nums[14] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA14;
  rgb_config.data_gpio_nums[15] = ESP_PANEL_LCD_PIN_NUM_RGB_DATA15;
  rgb_config.disp_gpio_num = ESP_PANEL_LCD_PIN_NUM_RGB_DISP;
  s_sem_vsync_end = xSemaphoreCreateBinary();
  s_sem_gui_ready = xSemaphoreCreateBinary();
  rgb_config.bounce_buffer_size_px = 10 * ESP_PANEL_LCD_WIDTH;
  rgb_config.flags.fb_in_psram = true;
  esp_lcd_new_rgb_panel(&rgb_config, &panel_handle);
  esp_lcd_rgb_panel_event_callbacks_t cbs = {
    .on_vsync = example_on_vsync_event,
  };
  esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL);
  esp_lcd_panel_reset(panel_handle);
  esp_lcd_panel_init(panel_handle);
}



static void lcd_vsync_wait_before_draw(void) {
  if (s_sem_gui_ready && s_sem_vsync_end) {
    xSemaphoreGive(s_sem_gui_ready);
    xSemaphoreTake(s_sem_vsync_end, pdMS_TO_TICKS(50));
  }
}

void LCD_Init() {
  ST7701_Reset();
  ST7701_Init();
  Touch_Init();
  Backlight_Init();
}

void LCD_addWindow(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend,uint8_t* color) {
  Xend = Xend + 1;
  Yend = Yend + 1;
  if (Xend >= ESP_PANEL_LCD_WIDTH)
    Xend = ESP_PANEL_LCD_WIDTH;
  if (Yend >= ESP_PANEL_LCD_HEIGHT)
    Yend = ESP_PANEL_LCD_HEIGHT;

  lcd_vsync_wait_before_draw();
  esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, color);
}

// backlight
uint8_t LCD_Backlight = 100;
void Backlight_Init()
{
  // Steady pure DC to eliminate 20kHz PWM ripple on adjacent RGB data traces
  pinMode(LCD_Backlight_PIN, OUTPUT);
  digitalWrite(LCD_Backlight_PIN, HIGH);
}

void Set_Backlight(uint8_t Light)
{
  if (Light > 0) {
    digitalWrite(LCD_Backlight_PIN, HIGH);
  } else {
    digitalWrite(LCD_Backlight_PIN, LOW);
  }
}
