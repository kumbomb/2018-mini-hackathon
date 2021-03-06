#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <peripheral_io.h>
#include <time.h>
#include <sys/time.h>
#include "log.h"
#include "resource/resource_1602A_LCD.h"
#include "resource/resource_util.h"
typedef enum {
	LCD_STATE_NONE,
	LCD_STATE_CONFIGURED,
	LCD_STATE_RUNNING,
} lcd_state_e;

typedef struct __lcd_data{
	int bits, rows, cols ;
	unsigned int rs_pin, strb_pin ;
	unsigned int data_pins [8] ;
	peripheral_gpio_h rs_pin_h;
	peripheral_gpio_h strb_pin_h;
	peripheral_gpio_h data_pins_h[8];
	lcd_state_e lcd_state;
	int cx, cy ;
} lcd_data;

static lcd_data lcds [LCD_MAX] = {
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LCD_STATE_NONE, 0, 0}
};
static int lcd_control ;
static const int row_off [4] = { 0x00, 0x40, 0x14, 0x54 } ;

void resource_1602A_LCD_close(lcd_id_e id){
	int i;
	if(lcds[id].rs_pin_h){
		peripheral_gpio_close(lcds[id].rs_pin_h);
		lcds[id].rs_pin_h = NULL;
	}

	if(lcds[id].strb_pin_h){
		peripheral_gpio_close(lcds[id].strb_pin_h);
		lcds[id].strb_pin_h = NULL;
	}

	for (i = 0 ; i < lcds[id].bits ; ++i){
		if(lcds[id].data_pins_h[i]){
			peripheral_gpio_close(lcds[id].data_pins_h[i]);
			lcds[id].data_pins_h[i] = NULL;
		}
	}
	lcds[id].lcd_state = LCD_STATE_NONE;
}
void resource_1602A_LCD_close_all(){
	int i;
	for(i = 0; i< LCD_MAX; i++){
		resource_1602A_LCD_close(i);
	}
}

static int __resource_1602A_LCD_strobe (const lcd_id_e id){
	int ret = 0;
	ret = peripheral_gpio_write(lcds[id].strb_pin_h, 1);
	if(ret != PERIPHERAL_ERROR_NONE){
		_E("failed to set value[1] strb pin");
		return -1;
	}
	resource_util_delay_microseconds(50) ;
	ret = peripheral_gpio_write(lcds[id].strb_pin_h, 0);
	if(ret != PERIPHERAL_ERROR_NONE){
		_E("failed to set value[0] strb pin");
		return -1;
	}
	resource_util_delay_microseconds(50) ;
	return 0;
}

static int __resource_1602A_LCD_send_data_cmd (const lcd_id_e id, unsigned char data){
	register unsigned char my_data = data;
	unsigned char i, d4;
	int ret = 0;
	if (lcds[id].bits == 4){
		d4 = (my_data >> 4) & 0x0F;

		for (i = 0 ; i < 4 ; ++i){
			ret = peripheral_gpio_write(lcds[id].data_pins_h[i], (d4 & 1));
			if(ret != PERIPHERAL_ERROR_NONE){
				_E("failed to set value[0] col[%d] pin", i);
				return -1;
			}
			d4 >>= 1 ;
		}
		ret = __resource_1602A_LCD_strobe(id);
		if(ret){
			_E("failed to __lcd_strobe()");
			return -1;
		}

		d4 = my_data & 0x0F ;
		for (i = 0 ; i < 4 ; ++i){
			ret = peripheral_gpio_write(lcds[id].data_pins_h[i], (d4 & 1));
			if(ret != PERIPHERAL_ERROR_NONE){
				_E("failed to set value[0] col[%d] pin", i);
				return -1;
			}
			d4 >>= 1 ;
		}
	}
	else{
		for (i = 0 ; i < 8 ; ++i){
			ret = peripheral_gpio_write(lcds[id].data_pins_h[i], (my_data & 1));
			if(ret != PERIPHERAL_ERROR_NONE){
				_E("failed to set value[0] col[%d] pin", i);
				return -1;
			}
			my_data >>= 1 ;
		}
	}
	ret = __resource_1602A_LCD_strobe(id) ;
	if(ret){
		_E("failed to __lcd_strobe()");
		return -1;
	}
	return 0;
}

static int __resource_1602A_LCD_put_command (const lcd_id_e id, unsigned char command){
	int ret = 0;
	ret = peripheral_gpio_write(lcds[id].rs_pin_h, 0);
	if(ret != PERIPHERAL_ERROR_NONE){
		_E("failed to set value[0] rs pin");
		return -1;
	}
	ret = __resource_1602A_LCD_send_data_cmd(id, command);
	if(ret){
		_E("failed to __lcd_send_data_cmd()");
		return -1;
	}
	resource_util_delay(2);
	return 0;
}

static int __resource_1602A_LCD_put_4_command (const lcd_id_e id, unsigned char command){
	register unsigned char my_command = command ;
	register unsigned char i ;
	int ret = 0;

	ret = peripheral_gpio_write(lcds[id].rs_pin_h, 0);
	if(ret != PERIPHERAL_ERROR_NONE){
		_E("failed to set value[0] rs[%d] pin", lcds[id].rs_pin);
		return -1;
	}

	for (i = 0 ; i < 4 ; ++i){
		ret = peripheral_gpio_write(lcds[id].data_pins_h[i], (my_command & 1));
		if(ret != PERIPHERAL_ERROR_NONE){
			_E("failed to set value[%d] data[%d] pin", (my_command & 1), lcds[id].data_pins[i]);
			return -1;
		}
		my_command >>= 1 ;
	}
	ret = __resource_1602A_LCD_strobe(id);
	if(ret){
		_E("failed to __lcd_strobe()");
		return -1;
	}
	return 0;
}

int __resource_1602A_LCD_home (const lcd_id_e id){
	int ret=0;
	ret = __resource_1602A_LCD_put_command(id, LCD_HOME);
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	lcds[id].cx = lcds[id].cy = 0 ;
	resource_util_delay(5);
	return 0;
}

int __resource_1602A_LCD_clear (const lcd_id_e id){
	int ret = 0;
	ret = __resource_1602A_LCD_put_command(id, LCD_CLEAR);
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	ret = __resource_1602A_LCD_put_command(id, LCD_HOME) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	lcds[id].cx = lcds[id].cy = 0 ;
	resource_util_delay(5) ;
	return 0;
}

int __resource_1602A_LCD_display (const lcd_id_e id, int state){
	int ret = 0;
	if (state)
		lcd_control |=  LCD_DISPLAY_CTRL ;
	else
		lcd_control &= ~LCD_DISPLAY_CTRL ;

	ret = __resource_1602A_LCD_put_command(id, LCD_CTRL | lcd_control) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	return 0;
}

int __resource_1602A_LCD_cursor (const lcd_id_e id, int state){
	int ret = 0;
	if (state)
		lcd_control |=  LCD_CURSOR_CTRL ;
	else
		lcd_control &= ~LCD_CURSOR_CTRL ;

	ret = __resource_1602A_LCD_put_command(id, LCD_CTRL | lcd_control);
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	return 0;
}

int __resource_1602A_LCD_cursor_blink (const lcd_id_e id, int state){
	int ret = 0;
	if (state)
		lcd_control |=  LCD_BLINK_CTRL ;
	else
		lcd_control &= ~LCD_BLINK_CTRL ;

	ret = __resource_1602A_LCD_put_command(id, LCD_CTRL | lcd_control) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	return 0;
}

int __resource_1602A_LCD_send_command (const lcd_id_e id, unsigned char command){
	int ret = 0;
	ret = __resource_1602A_LCD_put_command(id, command) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	return 0;
}

int __resource_set_defalut_1602A_LCD_configuration_by_id(lcd_id_e id)
{
	switch(id){
	case LCD_ID_1:
		lcds[id].rs_pin	= DEFAULT_RS_PIN;
		lcds[id].strb_pin = DEFAULT_STRB_PIN;
		lcds[id].bits = DEFAULT_BITS;
		lcds[id].rows = DEFAULT_ROW;
		lcds[id].cols = DEFAULT_COL;
		lcds[id].cx = 0;
		lcds[id].cy = 0;
		lcds[id].data_pins[0] = DEFAULT_D0_PIN;
		lcds[id].data_pins[1] = DEFAULT_D1_PIN;
		lcds[id].data_pins[2] = DEFAULT_D2_PIN;
		lcds[id].data_pins[3] = DEFAULT_D3_PIN;
		lcds[id].data_pins[4] = DEFAULT_D4_PIN;
		lcds[id].data_pins[5] = DEFAULT_D5_PIN;
		lcds[id].data_pins[6] = DEFAULT_D6_PIN;
		lcds[id].data_pins[7] = DEFAULT_D7_PIN;
		break;
	default:
		_E("Unkwon ID[%d]", id);
			return -1;
		break;
	}
	lcds[id].lcd_state = LCD_STATE_CONFIGURED;
	return 0;
}

int resource_set_1602A_LCD_configuration(lcd_id_e id, int rows, int cols, int bits,
		int rs, int strb, int d0, int d1, int d2, int d3, int d4, int d5, int d6, int d7 ){

	if(lcds[id].lcd_state > LCD_STATE_CONFIGURED){
		_E("cannot set configuration lcd[%d] in this state[%d]",
			id, lcds[id].lcd_state);
		return -1;
	}

	lcds[id].rs_pin	= rs;
	lcds[id].strb_pin = strb;
	lcds[id].bits = bits;
	lcds[id].rows = rows;
	lcds[id].cols = cols;
	lcds[id].cx = 0;
	lcds[id].cy = 0;
	lcds[id].data_pins[0] = d0;
	lcds[id].data_pins[1] = d1;
	lcds[id].data_pins[2] = d2;
	lcds[id].data_pins[3] = d3;
	lcds[id].data_pins[4] = d4;
	lcds[id].data_pins[5] = d5;
	lcds[id].data_pins[6] = d6;
	lcds[id].data_pins[7] = d7;
	lcds[id].lcd_state = LCD_STATE_CONFIGURED;
	return 0;
}

int __resource_1602A_LCD_init_by_id(lcd_id_e id){
	int ret = 0;
	unsigned char func;
	int i ;

	if(lcds[id].lcd_state == LCD_STATE_NONE){
		ret = __resource_set_defalut_1602A_LCD_configuration_by_id(id);
		if(ret){
			_E("failed to __set_defalut_lcd_configuration()");
			return -1;
		}
	}

	ret = peripheral_gpio_open(lcds[id].rs_pin, &lcds[id].rs_pin_h);
	if (ret == PERIPHERAL_ERROR_NONE){
		peripheral_gpio_set_direction(lcds[id].rs_pin_h,
			PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW);
	}
	else {
		_E("failed to open gpio rs pin[%u]", lcds[id].rs_pin);
		goto ERROR;
	}

	ret = peripheral_gpio_open(lcds[id].strb_pin, &lcds[id].strb_pin_h);
	if (ret == PERIPHERAL_ERROR_NONE){
		peripheral_gpio_set_direction(lcds[id].strb_pin_h,
			PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW);
	}
	else {
		_E("failed to open gpio strb pin[%u]", lcds[id].strb_pin_h);
		goto ERROR;
	}

	for (i = 0 ; i < lcds[id].bits ; ++i){
		ret = peripheral_gpio_open(lcds[id].data_pins[i], &lcds[id].data_pins_h[i]);
		if (ret == PERIPHERAL_ERROR_NONE){
			peripheral_gpio_set_direction(lcds[id].data_pins_h[i],
				PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW);
		}
		else {
			_E("failed to open gpio data[%d] pin[%u]", i, lcds[id].data_pins[i]);
			goto ERROR;
		}
	}
	resource_util_delay(35) ;

	if (lcds[id].bits == 4){
		func = LCD_FUNC | LCD_FUNC_DL ;
		ret = __resource_1602A_LCD_put_4_command(id, func >> 4);
		if(ret){
			_E("failed to __lcd_put_4_command()");
			return -1;
		}
		resource_util_delay(35);
		ret = __resource_1602A_LCD_put_4_command(id, func >> 4);
		if(ret){
			_E("failed to __lcd_put_4_command()");
			return -1;
		}
		resource_util_delay(35);
		ret = __resource_1602A_LCD_put_4_command(id, func >> 4);
		if(ret){
			_E("failed to __lcd_put_4_command()");
			return -1;
		}
		resource_util_delay(35);
		func = LCD_FUNC ;
		ret = __resource_1602A_LCD_put_4_command(id, func >> 4);
		if(ret){
			_E("failed to __lcd_put_4_command()");
			return -1;
		}
		resource_util_delay(35);
		lcds[id].bits = 4 ;
	}
	else{
		func = LCD_FUNC | LCD_FUNC_DL ;
		ret = __resource_1602A_LCD_put_command(id, func);
		if(ret){
			_E("failed to __lcd_put_command()");
			return -1;
		}
		resource_util_delay(35);
		ret = __resource_1602A_LCD_put_command(id, func);
		if(ret){
			_E("failed to __lcd_put_command()");
			return -1;
		}
		resource_util_delay(35);
		ret = __resource_1602A_LCD_put_command(id, func);
		if(ret){
			_E("failed to __lcd_put_command()");
			return -1;
		}
		resource_util_delay(35);
	}

	if (lcds[id].rows > 1){
		func |= LCD_FUNC_N ;
		ret = __resource_1602A_LCD_put_command(id, func);
		if(ret){
			_E("failed to __lcd_put_command()");
			return -1;
		}
		resource_util_delay(35);
	}

	ret = __resource_1602A_LCD_display(id, TRUE) ;
	if(ret){
		_E("failed to __lcd_display()");
		return -1;
	}
	ret = __resource_1602A_LCD_cursor(id, FALSE) ;
	if(ret){
		_E("failed to __lcd_cursor()");
		return -1;
	}
	ret = __resource_1602A_LCD_cursor_blink(id, FALSE);
	if(ret){
		_E("failed to __lcd_blink()");
		return -1;
	}
	ret = __resource_1602A_LCD_clear(id) ;
	if(ret){
		_E("failed to __lcd_clear()");
		return -1;
	}

	ret = __resource_1602A_LCD_put_command(id, LCD_ENTRY   | LCD_ENTRY_ID) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	ret = __resource_1602A_LCD_put_command(id, LCD_CDSHIFT | LCD_CDSHIFT_RL) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}
	lcds[id].lcd_state = LCD_STATE_RUNNING;
	return 0;

ERROR:
	if(lcds[id].rs_pin_h){
		peripheral_gpio_close(lcds[id].rs_pin_h);
		lcds[id].rs_pin_h = NULL;
	}

	if(lcds[id].strb_pin_h){
		peripheral_gpio_close(lcds[id].strb_pin_h);
		lcds[id].strb_pin_h = NULL;
	}

	for (i = 0 ; i < lcds[id].bits ; ++i){
		if(lcds[id].data_pins_h[i]){
			peripheral_gpio_close(lcds[id].data_pins_h[i]);
			lcds[id].data_pins_h[i] = NULL;
		}
	}
	lcds[id].lcd_state = LCD_STATE_NONE;
	return -1;
}

int resource_1602A_LCD_position (const lcd_id_e id, int x, int y)
{
	int ret = 0;
	if(lcds[id].lcd_state <= LCD_STATE_CONFIGURED){
		ret = __resource_1602A_LCD_init_by_id(id);
		if(ret){
			_E("failed to __init_lcd_by_id()");
			return -1;
		}
	}
	if ((x > lcds[id].cols) || (x < 0))
		return -1;
	if ((y > lcds[id].rows) || (y < 0))
		return -1;

	ret = __resource_1602A_LCD_put_command(id, x + (LCD_DGRAM | row_off[y])) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}

	lcds[id].cx = x ;
	lcds[id].cy = y ;
	return 0;
}

int resource_1602A_LCD_char_def(const lcd_id_e id, int index, unsigned char data [8]){
	int i;
	int ret = 0;
	ret = __resource_1602A_LCD_put_command(id, LCD_CGRAM | ((index & 7) << 3)) ;
	if(ret){
		_E("failed to __lcd_put_command()");
		return -1;
	}

	ret = peripheral_gpio_write(lcds[id].rs_pin_h, 1);
	if(ret != PERIPHERAL_ERROR_NONE){
		_E("failed to set value[1] rs[%d] pin", lcds[id].rs_pin);
		ret = -1;
		return ret;
	}
	for (i = 0 ; i < 8 ; ++i){
		ret = __resource_1602A_LCD_send_data_cmd(id, data[i]) ;
		if(ret){
			_E("failed to __lcd_send_data_cmd()");
			return -1;
		}
	}
	return 0;
}

int resource_1602A_LCD_putchar (const lcd_id_e id, unsigned char data){
	int ret = 0;
	if(lcds[id].lcd_state <= LCD_STATE_CONFIGURED){
		ret = __resource_1602A_LCD_init_by_id(id);
		if(ret){
			_E("failed to __init_lcd_by_id()");
			return -1;
		}
		ret = resource_1602A_LCD_position(id, 0, 0);
		if(ret){
			_E("failed to __position_lcd_by_id()");
			return -1;
		}
	}
	ret = peripheral_gpio_write(lcds[id].rs_pin_h, 1);
	if(ret != PERIPHERAL_ERROR_NONE){
		_E("failed to set value[1] rs[%d] pin", lcds[id].rs_pin);
		ret = -1;
		return ret;
	}
	ret = __resource_1602A_LCD_send_data_cmd(id, data) ;
	if(ret){
		_E("failed to __lcd_send_data_cmd()");
		return -1;
	}
	if (++lcds[id].cx == lcds[id].cols){
		lcds[id].cx = 0 ;
		if (++lcds[id].cy == lcds[id].rows)
			lcds[id].cy = 0 ;
		ret = __resource_1602A_LCD_put_command(id, lcds[id].cx + (LCD_DGRAM | row_off [lcds[id].cy])) ;
		if(ret){
			_E("failed to __lcd_put_command()");
			return -1;
		}
	}
	return 0;
}

int resource_1602A_LCD_puts (const lcd_id_e id, const char *string){
	int ret = 0;
	while (*string){
		ret = resource_1602A_LCD_putchar(id, *string++) ;
		if(ret){
			_E("failed to __lcd_putchar()");
			return -1;
		}
	}
	return 0;
}

int resource_1602A_LCD_printf (const lcd_id_e id, const char *message, ...)
{
	va_list argp ;
	char buffer [1024] ;
	int ret = 0;

	va_start (argp, message) ;
	vsnprintf (buffer, 1023, message, argp) ;
	va_end (argp) ;

	ret = resource_1602A_LCD_puts(id, buffer) ;
	if(ret){
		_E("failed to __lcd_puts()");
		return -1;
	}
	return 0;
}
