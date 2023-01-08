 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with the Touch Screen handlers

 public:
  uint8_t  getTouch(uint16_t *x, uint16_t *y, uint16_t threshold = 600);

  void     convertRawXY(uint16_t *x, uint16_t *y);
           // Run screen calibration and test, report calibration values to the serial port
  void     calibrateTouch(uint16_t *data, uint32_t color_fg, uint32_t color_bg, uint8_t size);
           // Set the screen calibration values
  void     setTouch(uint16_t *data);
  void     xpt2046_init(void);

 private:
           // Private function to validate a touch, allow settle time and reduce spurious coordinates
  uint8_t  validTouch(uint16_t *x, uint16_t *y, uint16_t threshold = 600);

           // Initialise with example calibration values so processor does not crash if setTouch() not called in setup()
  uint16_t touchCalibration_x0 = 300, touchCalibration_x1 = 3600, touchCalibration_y0 = 300, touchCalibration_y1 = 3600;
  uint8_t  touchCalibration_rotate = 1, touchCalibration_invert_x = 2, touchCalibration_invert_y = 0;

  uint32_t _pressTime;        // Press and hold time-out
  uint16_t _pressX, _pressY;  // For future use (last sampled calibrated coordinates)

  uint16_t  xpt2046_gpio_spi_read_reg(uint8_t reg);
  void      xpt2046_gpio_Write_Byte(uint8_t num);
  uint16_t  TP_Read_XOY(uint8_t xy);
