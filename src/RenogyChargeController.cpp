/*
  RenogyChargeController.cpp - Library for communicating with Renogy solar charge controllers over serial.
  Created by Noah Mollerstuen, June 26, 2024.
  Released under the MIT license.

  Forked from wrybread's ESP32ArduinoRenogy: https://github.com/wrybread/ESP32ArduinoRenogy
*/

#include "RenogyChargeController.h"
#include <ModbusMaster.h>

/*
Number of registers to check. I think all Renogy controllers have 35
data registers (not all of which are used) and 17 info registers.
*/
const uint32_t num_data_registers = 35;
const uint32_t num_info_registers = 17;

const int modbus_address = 255;

RenogyChargeController::RenogyChargeController(HardwareSerial &serial)
: _serial_stream(serial)
{
	_node = ModbusMaster();
};

void RenogyChargeController::begin()
{
	_serial_stream.begin(9600, SERIAL_8N1);
	_node.begin(modbus_address, _serial_stream);
};

bool RenogyChargeController::read_data_registers()
{
	uint8_t j, result;
	uint16_t data_registers[num_data_registers];
	char buffer1[40], buffer2[40];
	uint8_t raw_data;

	result = _node.readHoldingRegisters(0x100, num_data_registers);
	if (result == _node.ku8MBSuccess)
	{
		renogy_data.controller_connected = true;
		for (j = 0; j < num_data_registers; j++)
		{
			data_registers[j] = _node.getResponseBuffer(j);
		}

		renogy_data.battery_soc = data_registers[0];
		renogy_data.battery_voltage = data_registers[1] * .1; // will it crash if data_registers[1] doesn't exist?
		renogy_data.battery_charging_amps = data_registers[2] * .1;

		renogy_data.battery_charging_watts = renogy_data.battery_voltage * renogy_data.battery_charging_amps;

		// 0x103 returns two bytes, one for battery and one for controller temp in c
		uint16_t raw_data = data_registers[3]; // eg 5913
		renogy_data.controller_temperature = raw_data / 256;
		renogy_data.battery_temperature = raw_data % 256;
		// for convenience, fahrenheit versions of the temperatures
		renogy_data.controller_temperatureF = (renogy_data.controller_temperature * 1.8) + 32;
		renogy_data.battery_temperatureF = (renogy_data.battery_temperature * 1.8) + 32;

		renogy_data.load_voltage = data_registers[4] * .1;
		renogy_data.load_amps = data_registers[5] * .01;
		renogy_data.load_watts = data_registers[6];
		renogy_data.solar_panel_voltage = data_registers[7] * .1;
		renogy_data.solar_panel_amps = data_registers[8] * .01;
		renogy_data.solar_panel_watts = data_registers[9];
		// Register 0x10A - Turn on load, write register, unsupported in wanderer - 10
		renogy_data.min_battery_voltage_today = data_registers[11] * .1;
		renogy_data.max_battery_voltage_today = data_registers[12] * .1;
		renogy_data.max_charging_amps_today = data_registers[13] * .01;
		renogy_data.max_discharging_amps_today = data_registers[14] * .1;
		renogy_data.max_charge_watts_today = data_registers[15];
		renogy_data.max_discharge_watts_today = data_registers[16];
		renogy_data.charge_amphours_today = data_registers[17];
		renogy_data.discharge_amphours_today = data_registers[18];
		renogy_data.charge_watthours_today = data_registers[19];
		renogy_data.discharge_watthours_today = data_registers[20];
		renogy_data.controller_uptime_days = data_registers[21];
		renogy_data.total_battery_overcharges = data_registers[22];
		renogy_data.total_battery_fullcharges = data_registers[23];
		renogy_data.last_update_time = millis();

		// Add these registers:
		// Registers 0x118 to 0x119- Total Charging Amp-Hours - 24/25
		// Registers 0x11A to 0x11B- Total Discharging Amp-Hours - 26/27
		// Registers 0x11C to 0x11D- Total Cumulative power generation (kWH) - 28/29
		// Registers 0x11E to 0x11F- Total Cumulative power consumption (kWH) - 30/31
		// Register 0x120 - Load Status, Load Brightness, Charging State - 32
		// Registers 0x121 to 0x122 - Controller fault codes - 33/34

		return true;
	}
	else
	{
		// Reset some values if we don't get a reading
		renogy_data.controller_connected = false;
		renogy_data.battery_voltage = 0;
		renogy_data.battery_charging_amps = 0;
		renogy_data.battery_soc = 0;
		renogy_data.battery_charging_amps = 0;
		renogy_data.controller_temperature = 0;
		renogy_data.battery_temperature = 0;
		renogy_data.solar_panel_amps = 0;
		renogy_data.solar_panel_watts = 0;
		renogy_data.battery_charging_watts = 0;

		return false;
	}
}

bool RenogyChargeController::read_info_registers()
{
	uint8_t j, result;
	uint16_t info_registers[num_info_registers];
	char buffer1[40], buffer2[40];
	uint8_t raw_data;

	result = _node.readHoldingRegisters(0x00A, num_info_registers);
	if (result == _node.ku8MBSuccess)
	{
		for (j = 0; j < num_info_registers; j++)
		{
			info_registers[j] = _node.getResponseBuffer(j);
		}

		// read and process each value
		// Register 0x0A - Controller voltage and Current Rating - 0
		// Not sure if this is correct. I get the correct amp rating for my Wanderer 30 (30 amps), but I get a voltage rating of 0 (should be 12v)
		raw_data = info_registers[0];
		renogy_info.voltage_rating = raw_data / 256;
		renogy_info.amp_rating = raw_data % 256;
		renogy_info.wattage_rating = renogy_info.voltage_rating * renogy_info.amp_rating;
		// Serial.println("raw ratings = " + String(raw_data));
		// Serial.println("Voltage rating: " + String(renogy_info.voltage_rating));
		// Serial.println("amp rating: " + String(renogy_info.amp_rating));

		// Register 0x0B - Controller discharge current and type - 1
		raw_data = info_registers[1];
		renogy_info.discharge_amp_rating = raw_data / 256; // not sure if this should be /256 or /100
		renogy_info.type = raw_data % 256;				   // not sure if this should be /256 or /100

		// Registers 0x0C to 0x13 - Product Model String - 2-9
		//  Here's how the nodeJS project handled this:
		/*
		let modelString = '';
		for (let i = 0; i <= 7; i++) {
			rawData[i+2].toString(16).match(/.{1,2}/g).forEach( x => {
				modelString += String.fromCharCode(parseInt(x, 16));
			});
		}
		this.controllerModel = modelString.replace(' ','');
		*/

		// Registers 0x014 to 0x015 - Software Version - 10-11
		itoa(info_registers[10], buffer1, 10);
		itoa(info_registers[11], buffer2, 10);
		strcat(buffer1, buffer2); // should put a divider between the two strings?
		strcpy(renogy_info.software_version, buffer1);
		// Serial.println("Software version: " + String(renogy_info.software_version));

		// Registers 0x016 to 0x017 - Hardware Version - 12-13
		itoa(info_registers[12], buffer1, 10);
		itoa(info_registers[13], buffer2, 10);
		strcat(buffer1, buffer2); // should put a divider between the two strings?
		strcpy(renogy_info.hardware_version, buffer1);
		// Serial.println("Hardware version: " + String(renogy_info.hardware_version));

		// Registers 0x018 to 0x019 - Product Serial Number - 14-15
		//  I don't think this is correct... Doesn't match serial number printed on my controller
		itoa(info_registers[14], buffer1, 10);
		itoa(info_registers[15], buffer2, 10);
		strcat(buffer1, buffer2); // should put a divider between the two strings?
		strcpy(renogy_info.serial_number, buffer1);
		// Serial.println("Serial number: " + String(renogy_info.serial_number)); // (I don't think this is correct)

		renogy_info.modbus_address = info_registers[16];
		renogy_info.last_update_time = millis();

		return true;
	}
	else
	{
		return false;
	}
}

// control the load pins on Renogy charge controllers that have them
void RenogyChargeController::set_load(bool state)
{
	if (state == 1)
		_node.writeSingleRegister(0x010A, 1); // turn on load
	else
		_node.writeSingleRegister(0x010A, 0); // turn off load
}

static uint32_t i;
void RenogyChargeController::update()
{
	i++;

	// set word 0 of TX buffer to least-significant word of counter (bits 15..0)
	_node.setTransmitBuffer(0, lowWord(i));
	// set word 1 of TX buffer to most-significant word of counter (bits 31..16)
	_node.setTransmitBuffer(1, highWord(i));
}