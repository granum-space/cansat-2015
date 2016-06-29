/*
 * opt3001.c

 *
 *  Created on: 03 июня 2016 г.
 *      Author: developer
 */

#include <util/delay.h>
#include <avr/io.h>
#include "opt3001.h"
#include "i2c.h"

#define OPT3001_ADDR 0x10  // АДРЕСС ДАТЧИКА
#define ADDR_REG 0x01 // адрес регистра в котором лежит конфигурация в hex
#define ADDR_RES 0x00 //адрес регистра из которого мы хотим прочитать результат в hex
#define BYN_OPT (1 << 5)
#define BYN_OPT2 (1 << 7)
#define Low_Limit_ADDR 0x02 // адрес значения нижнего предела
#define Hight_Limit_ADDR 0x03 // адресс згасения вернего предела (верхний предел в принципе не не=ужен нам)

// биты конфигурационного регистра:
// ========================================================
// Биты управления диапазоном измерения OPT
#define OPT_CFG_RN0 12
#define OPT_CFG_RN1 13
#define OPT_CFG_RN2 14
#define OPT_CFG_RN3 15
// комбинации указаны в даташите, таблица 9, на странице 20

// время измерения.
#define OPT_CFG_CT 11
// 0 => 100ms, 1 => 800ms

// режим измерения
#define OPT_CFG_M0 10
#define OPT_CFG_M1 9
// 00 => режим ожидания; 01 => одиночное измерение; 10 или 11 => непрерывные измерения

// флаг переполнения
#define OPT_CFG_OVF 8
// когда в этом флаге 1 - это означает, что прибор "зашкалило" и действительное значение яркости больше чем
// зафиксированно

// флаг готовности результата
#define OPT_CFG_CRF 7
// читается как единица, когда преобразование заверешено. Сбрасывается повторным чтением/записью
// конфигурационного регистра.

// признак перехода за верхний предел освещенности
#define OPT_CFG_FH 6
// этот бит устанавливается в единичку, когда значение освещенности превышает значение
// заданное в регистре High Limit (0x03). Возможна генерация сигнала прерывания по этому событию

// признак перехода за нижний предел освещенности
#define OPT_CFG_FL 5
// этот бит устанавливается в единичку, когда значение освещенности становится меньше, чем значение
// заданное в регистре Low Limit (0x02). Возможна генерация сигнала прерывания по этому событию

// Режим "защелкивания прерывания"
#define OPT_CFG_L 4
// При единичке в этом бите, состояние прерывания наступает после N последовательных измерений,
// которые дают значение большее чем в High Limit регистре или меньшее чем Low Limit.
// при этом состояние прерывания сбрасывается только при чтении конфигурационного регистра
//
// при нуле в этом бите устройство входит в состояние прерывания после N последовательных измерений,
// которые дают значение больше, чем в High Limit регистре. Выход из состояния прерывания
// происходит после N измерений, которые дают значение меньшее чем в Low Limit регистре.
// чтение конфигурационного регистра флаг прерывания не снимает

// полярность сигнала прерывания
#define OPT_CFG_POL 3
// POL == 0. Состояние прерывания обозначается нулём на INT пине OPT3001
// POL == 1. Состояние прерывания обозначается единичкой на INT пине OPT3001

// маскирование экспоненты результата измерения.
#define OPT_CFG_ME 2
// управляет представлением результата измерения при заданных в ручную пределах измерения


// Управляет количеством измерений, по которым осуществляется переход в состояние прерывания
#define OPT_CFG_FC1 1
#define OPT_CFG_FC0 0
// 00 -> Одно измерение
// 01 -> Два измерения
// 10 -> Четыре измерения
// 11 -> Восемь измерений

// нижний предел
// экспонента
#define OPT_FL_LE3 15
#define OPT_FL_LE2 14
#define OPT_FL_LE1 13
#define OPT_FL_LE0 12
// мантиса
#define OPT_FL_TE11 11
#define OPT_FL_TE10 10
#define OPT_FL_TE9 9
#define OPT_FL_TE8 8
#define OPT_FL_TE7 7
#define OPT_FL_TE6 6
#define OPT_FL_TE5 5
#define OPT_FL_TE4 4
#define OPT_FL_TE3 3
#define OPT_FL_TE2 2
#define OPT_FL_TE1 1
#define OPT_FL_TE0 0

uint16_t cfg_reg;

int OPT_read(uint8_t ADDR_read, uint16_t * value){

	int flag = 0;

	flag = i2c_start();
	if(flag != 0)
		return flag;

	flag = i2c_send_slaw(OPT3001_ADDR, false);
	if(flag != 0){
		i2c_stop();
		return flag;
	}

	uint8_t target_register_addr = ADDR_read;
	flag = i2c_write(&target_register_addr, 1);
	if(flag != 0 ){
		i2c_stop();
		return flag;
	}

	i2c_start();
	flag = i2c_send_slaw(OPT3001_ADDR, true);
	    if (flag != 0){
	        i2c_stop();
	        return flag;
	 }

	 flag = i2c_read(&value, 2, true);

	 i2c_stop();
	 return flag;
}

uint16_t OPT_RESULT(){
	uint16_t result;
	while(1){
		OPT_read(ADDR_REG, &cfg_reg);
		if(cfg_reg & BYN_OPT2) break; // завершенно ли преобразование ацп
	}
	OPT_read(ADDR_RES, &result);
	return result;
}

int OPT_write(uint8_t ADDR_write, uint16_t * value){
	int flag = 0;

	flag = i2c_start();
	if(flag != 0)
		return flag;

	flag = i2c_send_slaw(OPT3001_ADDR, false);
	if(flag != 0){
		i2c_stop();
		return flag;
	}

	uint8_t target_register_addr = ADDR_write;
	flag = i2c_write(&target_register_addr, 1);
	if (flag != 0)
	{
		i2c_stop();
	    return flag;
	}
	flag = i2c_write(value, 2);
	i2c_stop();
	return flag;
}

void OPT_init(){

	// нижний лимит равен 40 люкс ( табл на стр 20 )
	//40 / 0,01 = 4000 в бинарном это  111110100000
	uint16_t FL_limit =
			(OPT_FL_LE3 << 0) | (OPT_FL_LE2 << 0)| (OPT_FL_LE1 << 0)|(OPT_FL_LE0 << 0) |
			(OPT_FL_TE11 << 1) | (OPT_FL_TE10 << 1) |(OPT_FL_TE9 << 1) | (OPT_FL_TE8 << 1 )|
			(OPT_FL_TE7 << 1) | (OPT_FL_TE6 << 0) | (OPT_FL_TE5 << 1) |(OPT_FL_TE4 << 0 )|
			(OPT_FL_TE3 << 0) | (OPT_FL_TE2 << 0) | (OPT_FL_TE1 << 0) | (OPT_FL_TE0 << 0);
	 cfg_reg =
	        // автоматическое определение пределов измерения
	        (0 << OPT_CFG_RN0) | (0 << OPT_CFG_RN1) | (1 << OPT_CFG_RN2) | (1 << OPT_CFG_RN3) |
	        // длительность измерений на 100мс
	        (0 << OPT_CFG_CT) |
	        // режим непрерывного измерения
	        (1 << OPT_CFG_M0) | (1 << OPT_CFG_M1) |
	        // флаг переполнения (OVF) не трогаем
	        // флаг заверешния преобразования (CRF) не трогаем
	        // флаги прерываний (FH и FL) не трогаем
	        // режим прерывания ставим на защелкиваемый (состяние удерживается до чтения из регистра конфигурации)
	        (1 << OPT_CFG_L) |
	        // полярность прерывания - единица означает активное прерывание
	        (1 << OPT_CFG_POL) |
	        // не маскируем экспоненциальную часть результата
	        (0 << OPT_CFG_ME) |
	        // ставим по максимуму (8) измерений превыщающих предел для перехода в прерывание
	        (1 << OPT_CFG_FC0) | (1 << OPT_CFG_FC1) ;

	// меняем конфигурацию
	OPT_write(ADDR_REG, &cfg_reg);  // отправлеям новую конфу на запись
	OPT_write(Low_Limit_ADDR, &FL_limit); // отправляем значение нижнего прдела на запись
}

uint8_t OPT_check(){
	uint16_t cfg_reg;
	OPT_read(ADDR_REG, &cfg_reg);
	if(cfg_reg & BYN_OPT) // стали ли мы измерять выше нижнего предела
		return 1;
	else
		return 0;
}



