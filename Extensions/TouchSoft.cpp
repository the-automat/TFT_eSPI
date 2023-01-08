// The following touch screen support code by maxpautsch was merged 1/10/17
// https://github.com/maxpautsch

// Define TOUCH_CS is the user setup file to enable this code

// A demo is provided in examples Generic folder

// Additions by Bodmer to double sample, use Z value to improve detection reliability
// and to correct rotation handling

// See license in root directory.
#define CMD_X_READ  0b10010000      //0x90
#define CMD_Y_READ  0b11010000      //0xD0
#define Read_Count		30			// 读取次数
#define TAG "XPT2046"



void TFT_eSPI::xpt2046_init(void)
{
    gpio_config_t irq_config = {
        .pin_bit_mask = BIT64(XPT2046_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&irq_config);
    /**/
    gpio_config_t miso_config = {
        .pin_bit_mask = BIT64(XPT2046_MISO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&miso_config);
    gpio_pad_select_gpio((gpio_num_t)XPT2046_MOSI);
    gpio_set_direction((gpio_num_t)XPT2046_MOSI, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio((gpio_num_t)XPT2046_CLK);
    gpio_set_direction((gpio_num_t)XPT2046_CLK, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio((gpio_num_t)XPT2046_CS);
    gpio_set_direction((gpio_num_t)XPT2046_CS, GPIO_MODE_OUTPUT);
    
    printf("%s->XPT2046 Initialization\n",TAG);
    assert(ret == ESP_OK);
}

uint16_t TFT_eSPI::TP_Read_XOY(uint8_t xy)
{
	uint8_t LOST_VAL = 1;//丢弃值  
	uint16_t i,j,temp,buf[Read_Count];
	uint32_t sum=0;
	for(i=0;i<Read_Count;i++){
		buf[i]=xpt2046_gpio_spi_read_reg(xy);
	}
	for(i=0;i<Read_Count-1; i++){//排序
		for(j=i+1;j<Read_Count;j++){
			if(buf[i]>buf[j]){//升序排列
				temp=buf[i];
				buf[i]=buf[j];
				buf[j]=temp;
			}
		}
	}
	sum=0;
	for(i=LOST_VAL;i<Read_Count-LOST_VAL;i++){
		sum+=buf[i];
	}
	temp = sum/(Read_Count-2*LOST_VAL);
	return temp;
}

void TFT_eSPI::xpt2046_gpio_Write_Byte(uint8_t num)    
{  
	uint8_t count=0;   
	for(count=0;count<8;count++){
		if(num&0x80){
			gpio_set_level((gpio_num_t)XPT2046_MOSI, 1);  
		}else{
			gpio_set_level((gpio_num_t)XPT2046_MOSI, 0);
		}  
		num<<=1; 
		gpio_set_level((gpio_num_t)XPT2046_CLK, 0);
		gpio_set_level((gpio_num_t)XPT2046_CLK, 1);
	}
}

uint16_t TFT_eSPI::xpt2046_gpio_spi_read_reg(uint8_t reg)
{
	uint8_t count=0;
	uint16_t ADValue=0;
	gpio_set_level((gpio_num_t)XPT2046_CLK, 0);		// 先拉低时钟 	 
	gpio_set_level((gpio_num_t)XPT2046_MOSI, 0); 	// 拉低数据线
	gpio_set_level((gpio_num_t)XPT2046_CS, 0); 		// 选中触摸屏IC
	xpt2046_gpio_Write_Byte(reg);		// 发送命令字
	ets_delay_us(6);					// ADS7846的转换时间最长为6us
	gpio_set_level((gpio_num_t)XPT2046_CLK, 0);
	ets_delay_us(1);
	gpio_set_level((gpio_num_t)XPT2046_CLK, 1);		// 给1个时钟，清除BUSY
	gpio_set_level((gpio_num_t)XPT2046_CLK, 0);
	for(count=0;count<16;count++){		// 读出16位数据,只有高12位有效
		ADValue<<=1; 	 
		gpio_set_level((gpio_num_t)XPT2046_CLK, 0);	// 下降沿有效
		gpio_set_level((gpio_num_t)XPT2046_CLK, 1);	
		if(gpio_get_level((gpio_num_t)XPT2046_MISO))ADValue++;
	}  	
	ADValue>>=4;						// 只有高12位有效.
	gpio_set_level((gpio_num_t)XPT2046_CS, 1);		// 释放片选
	return(ADValue);
}


/***************************************************************************************
** Function name:           validTouch
** Description:             read validated position. Return false if not pressed. 
***************************************************************************************/
#define _RAWERR 20 // Deadband error allowed in successive position samples
uint8_t TFT_eSPI::validTouch(uint16_t *x, uint16_t *y, uint16_t threshold){
	uint16_t ux = 0,uy = 0;
	uint8_t irq = gpio_get_level((gpio_num_t)XPT2046_IRQ);
	if (irq == 0) {
    // Serial.println("In validTouch and irq true!");
		ux = TP_Read_XOY(CMD_X_READ);   //5700   61700
		uy = TP_Read_XOY(CMD_Y_READ);   //3000   62300
    // Serial.print("X=");
    // Serial.print(ux);
    // Serial.print("Y=");
    // Serial.println(uy);
    *x = ux;
    *y = uy;
    return true;
  }
  return false;
}
  
/***************************************************************************************
** Function name:           getTouch
** Description:             read callibrated position. Return false if not pressed. 
***************************************************************************************/
#define Z_THRESHOLD 350 // Touch pressure threshold for validating touches
uint8_t TFT_eSPI::getTouch(uint16_t *x, uint16_t *y, uint16_t threshold){
  uint16_t x_tmp, y_tmp;
  
  if (threshold<20) threshold = 20;
  if (_pressTime > millis()) threshold=20;

  uint8_t n = 5;
  uint8_t valid = 0;
  while (n--)
  {
    if (validTouch(&x_tmp, &y_tmp, threshold)) valid++;;
  }

  if (valid<1) { _pressTime = 0; return false; }
  
  _pressTime = millis() + 50;

  convertRawXY(&x_tmp, &y_tmp);

  if (x_tmp >= _width || y_tmp >= _height) return false;

  _pressX = x_tmp;
  _pressY = y_tmp;
  *x = _pressX;
  *y = _pressY;
  return valid;
}

/***************************************************************************************
** Function name:           convertRawXY
** Description:             convert raw touch x,y values to screen coordinates 
***************************************************************************************/
void TFT_eSPI::convertRawXY(uint16_t *x, uint16_t *y)
{
  uint16_t x_tmp = *x, y_tmp = *y, xx, yy;

  if(!touchCalibration_rotate){
    xx=(x_tmp-touchCalibration_x0)*_width/touchCalibration_x1;
    yy=(y_tmp-touchCalibration_y0)*_height/touchCalibration_y1;
    if(touchCalibration_invert_x)
      xx = _width - xx;
    if(touchCalibration_invert_y)
      yy = _height - yy;
  } else {
    xx=(y_tmp-touchCalibration_x0)*_width/touchCalibration_x1;
    yy=(x_tmp-touchCalibration_y0)*_height/touchCalibration_y1;
    if(touchCalibration_invert_x)
      xx = _width - xx;
    if(touchCalibration_invert_y)
      yy = _height - yy;
  }
  *x = xx;
  *y = yy;
}

/***************************************************************************************
** Function name:           calibrateTouch
** Description:             generates calibration parameters for touchscreen. 
***************************************************************************************/
void TFT_eSPI::calibrateTouch(uint16_t *parameters, uint32_t color_fg, uint32_t color_bg, uint8_t size){
  int16_t values[] = {0,0,0,0,0,0,0,0};
  uint16_t x_tmp, y_tmp;



  for(uint8_t i = 0; i<4; i++){
    fillRect(0, 0, size+1, size+1, color_bg);
    fillRect(0, _height-size-1, size+1, size+1, color_bg);
    fillRect(_width-size-1, 0, size+1, size+1, color_bg);
    fillRect(_width-size-1, _height-size-1, size+1, size+1, color_bg);

    if (i == 5) break; // used to clear the arrows
    
    switch (i) {
      case 0: // up left
        drawLine(0, 0, 0, size, color_fg);
        drawLine(0, 0, size, 0, color_fg);
        drawLine(0, 0, size , size, color_fg);
        break;
      case 1: // bot left
        drawLine(0, _height-size-1, 0, _height-1, color_fg);
        drawLine(0, _height-1, size, _height-1, color_fg);
        drawLine(size, _height-size-1, 0, _height-1 , color_fg);
        break;
      case 2: // up right
        drawLine(_width-size-1, 0, _width-1, 0, color_fg);
        drawLine(_width-size-1, size, _width-1, 0, color_fg);
        drawLine(_width-1, size, _width-1, 0, color_fg);
        break;
      case 3: // bot right
        drawLine(_width-size-1, _height-size-1, _width-1, _height-1, color_fg);
        drawLine(_width-1, _height-1-size, _width-1, _height-1, color_fg);
        drawLine(_width-1-size, _height-1, _width-1, _height-1, color_fg);
        break;
      }

    // user has to get the chance to release
    if(i>0) delay(1000);

    for(uint8_t j= 0; j<8; j++){
      // Use a lower detect threshold as corners tend to be less sensitive
      while(!validTouch(&x_tmp, &y_tmp, Z_THRESHOLD/2));
      values[i*2  ] += x_tmp;
      values[i*2+1] += y_tmp;
      }
    values[i*2  ] /= 8;
    values[i*2+1] /= 8;
  }


  // from case 0 to case 1, the y value changed. 
  // If the measured delta of the touch x axis is bigger than the delta of the y axis, the touch and TFT axes are switched.
  touchCalibration_rotate = false;
  if(abs(values[0]-values[2]) > abs(values[1]-values[3])){
    touchCalibration_rotate = true;
    touchCalibration_x0 = (values[1] + values[3])/2; // calc min x
    touchCalibration_x1 = (values[5] + values[7])/2; // calc max x
    touchCalibration_y0 = (values[0] + values[4])/2; // calc min y
    touchCalibration_y1 = (values[2] + values[6])/2; // calc max y
  } else {
    touchCalibration_x0 = (values[0] + values[2])/2; // calc min x
    touchCalibration_x1 = (values[4] + values[6])/2; // calc max x
    touchCalibration_y0 = (values[1] + values[5])/2; // calc min y
    touchCalibration_y1 = (values[3] + values[7])/2; // calc max y
  }

  // in addition, the touch screen axis could be in the opposite direction of the TFT axis
  touchCalibration_invert_x = false;
  if(touchCalibration_x0 > touchCalibration_x1){
    values[0]=touchCalibration_x0;
    touchCalibration_x0 = touchCalibration_x1;
    touchCalibration_x1 = values[0];
    touchCalibration_invert_x = true;
  }
  touchCalibration_invert_y = false;
  if(touchCalibration_y0 > touchCalibration_y1){
    values[0]=touchCalibration_y0;
    touchCalibration_y0 = touchCalibration_y1;
    touchCalibration_y1 = values[0];
    touchCalibration_invert_y = true;
  }

  // pre calculate
  touchCalibration_x1 -= touchCalibration_x0;
  touchCalibration_y1 -= touchCalibration_y0;

  if(touchCalibration_x0 == 0) touchCalibration_x0 = 1;
  if(touchCalibration_x1 == 0) touchCalibration_x1 = 1;
  if(touchCalibration_y0 == 0) touchCalibration_y0 = 1;
  if(touchCalibration_y1 == 0) touchCalibration_y1 = 1;

  // export parameters, if pointer valid
  if(parameters != NULL){
    parameters[0] = touchCalibration_x0;
    parameters[1] = touchCalibration_x1;
    parameters[2] = touchCalibration_y0;
    parameters[3] = touchCalibration_y1;
    parameters[4] = touchCalibration_rotate | (touchCalibration_invert_x <<1) | (touchCalibration_invert_y <<2);
  }
}


/***************************************************************************************
** Function name:           setTouch
** Description:             imports calibration parameters for touchscreen. 
***************************************************************************************/
void TFT_eSPI::setTouch(uint16_t *parameters){
  touchCalibration_x0 = parameters[0];
  touchCalibration_x1 = parameters[1];
  touchCalibration_y0 = parameters[2];
  touchCalibration_y1 = parameters[3];

  if(touchCalibration_x0 == 0) touchCalibration_x0 = 1;
  if(touchCalibration_x1 == 0) touchCalibration_x1 = 1;
  if(touchCalibration_y0 == 0) touchCalibration_y0 = 1;
  if(touchCalibration_y1 == 0) touchCalibration_y1 = 1;

  touchCalibration_rotate = parameters[4] & 0x01;
  touchCalibration_invert_x = parameters[4] & 0x02;
  touchCalibration_invert_y = parameters[4] & 0x04;
}
