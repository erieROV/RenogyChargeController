#include <Arduino.h>
#include <RenogyChargeController.h>

RenogyChargeController charge_controller(Serial1);

void setup()
{
	Serial.begin(115200);
	Serial.println("Started!");

	charge_controller.begin();
}

void loop()
{
	charge_controller.update();
	charge_controller.read_data_registers();
	charge_controller.read_info_registers();

	Serial.println("Battery voltage: " + String(charge_controller.renogy_data.battery_voltage));
	Serial.println("Battery charge level: " + String(charge_controller.renogy_data.battery_soc) + "%");
	Serial.println("Panel wattage: " + String(charge_controller.renogy_data.solar_panel_watts));
	Serial.println("controller_temperatureF=" + String(charge_controller.renogy_data.controller_temperatureF));
	Serial.println("battery_temperatureF=" + String(charge_controller.renogy_data.battery_temperatureF));
	Serial.println("---");

	// turn the load on for 10 seconds
	// renogy_control_load(1)
	// delay(10000);
	// renogy_control_load(0)

	delay(1000);
}