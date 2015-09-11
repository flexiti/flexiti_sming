
#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/DHT/DHT.h>
#include <Libraries/OneWire/OneWire.h>
#include <Libraries/BMP180/BMP180.h>
#include <SmingCore/Network/TelnetServer.h>
#include "Services/CommandProcessing/CommandProcessingIncludes.h"
#include "Ntp_Sming.h"


// ---------------------------------------------------------------

#ifndef THINGSPEAK_API_KEY
#define THINGSPEAK_API_KEY	"1234567890"			// Put Your API key
#endif

#ifndef THINGSPEAK_SERVER
#define THINGSPEAK_SERVER "184.106.153.149"  		// direct ip or
//#define THINGSPEAK_SERVER "api.thingspeak.com"  	// name
#endif

#ifndef KEIL_SERVER
#define KEIL_SERVER "192.168.0.5:1000"		//it's my own server
#endif


#define KEIL_SERVER_EXIST 					//if exist data are send alternately to Thingspeak and Keil
											//if a comment, data is send only to Thingspeak


#ifndef WIFI_SSID
	#define WIFI_SSID "XXX" 			// Put you SSID and Password here
	#define WIFI_PWD "YYYYYY"
#endif

// ********************************************************************
// Program 3 option:
// ESP01 + DHT22  ( field6 hummanidty,field2 temperature ) -> uncomment only "DHT_22"
// ESP01 + DS1820 ( field5 temperature ) -> uncomment only "DS_18"
// ESP12 + DS1820 + BMP180 ( field1 tempDS, field7 tempBMP,field8 pressure )-> uncomment only "DS_18" and "BMP_180"
//
// ********************************************************************

#define DTH_22
//#define DS_18
//#define BMP_180

//----------------------------------------------------------------------
//----------------------------------------------------------------------
#ifdef DS_18
#define WORK_PIN 2 			// DS1820 on GPIO2
OneWire ds(WORK_PIN);
#endif

#ifdef BMP_180
BMP180 barometer;
#endif

#ifdef DTH_22
#define WORK_PIN 2 			// DHT22 on GPIO2
DHT dht(WORK_PIN, DHT22);
#endif

//----------------------------------------------------------------------

Timer procTimer;
Timer resetTimer;
DateTime dateTime;
DateTime SdateTime;
int sensorValue = 1;
HttpClient thingSpeak;
TelnetServer telnetServer;
int LastErr[2];
bool first = false;
int cctemp;
int ccpres;

ntpClientSming *NtpSming;


// Telenet reset command
void ResetCommand(String commandLine  ,CommandOutput* commandOutput)
{
	commandOutput->printf("You entered : '");
	commandOutput->printf(commandLine.c_str());
	commandOutput->printf("'\r\n");
	system_restart();
}
// Telenet info command
void EspInfoCommand(String commandLine  ,CommandOutput* commandOutput)
{
	commandOutput->printf("You entered : '");
	commandOutput->printf(commandLine.c_str());
	commandOutput->printf("'\r\n");

	Serial.println("---------------------");
#ifdef DS_18
	commandOutput->printf("\r\n********** DS18B20 *********\r\n");
#endif
#ifdef BMP_180
	commandOutput->printf("\r\n********** BMP180 **********\r\n");
#endif
#ifdef DTH_22
	commandOutput->printf("\r\n********** DHT22  **********\r\n");
#endif
	commandOutput->printf("\r\nMy IP  : ");
	commandOutput->printf("%s - %s\r\n",WifiStation.getIP().toString().c_str(),WifiStation.getMAC().c_str());
	commandOutput->printf("My gate: ");
	commandOutput->printf("%s\r\n",WifiStation.getNetworkGateway().toString().c_str());
	commandOutput->printf("My SSID: ");
	commandOutput->printf("%s\r\n\r\n",WifiStation.getSSID().c_str());
	commandOutput->printf("LastErr Keil: ");
	commandOutput->printf("%x\r\n",LastErr[0]);
	commandOutput->printf("LastErr Thing: ");
	commandOutput->printf("%x\r\n\r\n",LastErr[1]);
	commandOutput->printf("Sending counter     : ");
	commandOutput->printf("%d\r\n",sensorValue);
	commandOutput->printf("System start reason : %d\r\n", system_get_rst_info()->reason);
	commandOutput->printf("System reset time   : %s\r\n",SdateTime.toFullDateTimeString().c_str());
	commandOutput->printf("Aktual date and time: %s\r\n\r\n",SystemClock.getSystemTimeString().c_str());

	if (procTimer.isStarted())
		commandOutput->printf("Timer ON\r\n");
	else
		commandOutput->printf("Timer OFF\r\n");

	commandOutput->printf("Timer interval: %d\r\n\r\n",procTimer.getIntervalMs());


}


#ifdef BMP_180
void Read_BMP180 ()
{


	if(!barometer.EnsureConnected())
		Serial.println("Could not connect to BMP180.");

	// When we have connected, we reset the device to ensure a clean start.
   //barometer.SoftReset();
   // Now we initialize the sensor and pull the calibration data.
    barometer.Initialize();
	barometer.PrintCalibrationData();

	Serial.print("Start reading");

	// Retrive the current pressure in Pascals.
	long currentPressure = barometer.GetPressure();

	// Print out the Pressure.
	Serial.print("Pressure: ");
	Serial.print(currentPressure);
	Serial.print(" Pa");

	// Retrive the current temperature in degrees celcius.
	float currentTemperature = barometer.GetTemperature();

	// Print out the Temperature
	Serial.print("\tTemperature: ");
	Serial.print(currentTemperature);
	Serial.write(176);
	Serial.print("C");

	Serial.println(); // Start a new line.

	cctemp=(int)(currentTemperature*10);
	ccpres=(int)(currentPressure/10);
}
#endif






#ifdef DS_18
float Read_DS18()
{
byte i;
byte present = 0;
byte type_s;
byte data[12];
byte addr[8];
float celsius, fahrenheit;

ds.reset_search();
   delay(100);
if (!ds.search(addr))
{
	Serial.println("No addresses found.");
	Serial.println();
	ds.reset_search();
	delay(250);
	return 5000;
}

Serial.print("Thermometer ROM =");
for( i = 0; i < 8; i++)
{
	Serial.write(' ');
	Serial.print(addr[i], HEX);
}

if (OneWire::crc8(addr, 7) != addr[7])
{
  Serial.println("CRC is not valid!");
  return 5000;
}
Serial.println();

// the first ROM byte indicates which chip
switch (addr[0]) {
case 0x10:
  Serial.println("  Chip = DS18S20");  // or old DS1820
  type_s = 1;
  break;
case 0x28:
  Serial.println("  Chip = DS18B20");
  type_s = 0;
  break;
case 0x22:
  Serial.println("  Chip = DS1822");
  type_s = 0;
  break;
default:
  Serial.println("Device is not a DS18x20 family device.");
  return 5000;
}

ds.reset();
ds.select(addr);
ds.write(0x44, 1);        // start conversion, with parasite power on at the end

delay(1000);     // maybe 750ms is enough, maybe not
// we might do a ds.depower() here, but the reset will take care of it.

present = ds.reset();
ds.select(addr);
ds.write(0xBE);         // Read Scratchpad

Serial.print("  Data = ");
Serial.print(present, HEX);
Serial.print(" ");
for ( i = 0; i < 9; i++)
{
	// we need 9 bytes
	data[i] = ds.read();
	Serial.print(data[i], HEX);
	Serial.print(" ");
}
Serial.print(" CRC=");
Serial.print(OneWire::crc8(data, 8), HEX);
Serial.println();

// Convert the data to actual temperature
// because the result is a 16 bit signed integer, it should
// be stored to an "int16_t" type, which is always 16 bits
// even when compiled on a 32 bit processor.
int16_t raw = (data[1] << 8) | data[0];
if (type_s)
{
	raw = raw << 3; // 9 bit resolution default
	if (data[7] == 0x10)
	{
	  // "count remain" gives full 12 bit resolution
	  raw = (raw & 0xFFF0) + 12 - data[6];
	}
} else {
	byte cfg = (data[4] & 0x60);
	// at lower res, the low bits are undefined, so let's zero them
	if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
	else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
	else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
	//// default is 12 bit resolution, 750 ms conversion time
}

celsius = (float)raw / 16.0;
fahrenheit = celsius * 1.8 + 32.0;
Serial.print("  Temperature = ");
Serial.print(celsius);
Serial.print(" Celsius, ");
Serial.print(fahrenheit);
Serial.println(" Fahrenheit");
Serial.println();
return celsius;
}

#endif


void resetData()
{
	thingSpeak.reset();  // or other resets You may put here
						// now not working future
	Serial.println("My reset timer executed");
}

void onDataSent(HttpClient& client, bool successful)
{
	Serial.println("");
	Serial.print("Send Time   --> ");
	Serial.println(SystemClock.getSystemTimeString());
	Serial.print("onDataSent  --> ResponseCode: ");
	Serial.println(client.getReponseCode());
	Serial.print("onDataSent  --> ResponseString: ");
	Serial.println(client.getResponseString());
	Serial.print("onDataSent  --> PostBody: ");
	Serial.println(client.getPostBody());
	Serial.print("onDataSent  --> ResponseHeader: ");
	Serial.println(client.getResponseHeader("Server","none"));
	Serial.print("onDataSent  --> ResponseState: ");
	Serial.println(client.getConnectionState());
	Serial.print("onDataSent  --> Successful: ");
	if (successful)
	{
		Serial.println("Success ");
		LastErr[sensorValue&1]=client.getReponseCode();
	}
	else
	{
		Serial.println("Error");
		LastErr[sensorValue&1]=client.getReponseCode();

	}
	Serial.print("Free heap size = ");
	Serial.println (system_get_free_heap_size());
	Serial.println("");

//	if ((LastErr[0] == 200) && (LastErr[1] == 200))
	   resetTimer.restart();    //refresh connection timer, now not used


}

#ifdef DTH_22
void sendData()
{
	char hum[16];
	char temp[16];
	char temp2[256];

#ifdef KEIL_SERVER_EXIST
	sensorValue++;					// two or one server
#else
	sensorValue+=2;
#endif

	if (thingSpeak.isProcessing())
	{
      //  thingSpeak.reset();
        LastErr[sensorValue&1]=6;
		 Serial.println("LastErr=6");
		return; // We need to wait while request processing was completed
	}
	// Read our sensor value :)


	float h = dht.readHumidity();
		float t = dht.readTemperature();

		// check if returns are valid, if they are NaN (not a number) then something went wrong!
		if (isnan(t) || isnan(h))
		{
			Serial.println("Failed to read from DHT - not send");
		} else {
			 Serial.println(" ");
			if (sensorValue & 1)
			   Serial.println("THINGSPEAK");
			else
			   Serial.println("KEIL");
			Serial.print("Humidity: ");
			Serial.print(h);
			Serial.print(" %\t");
			Serial.print("Temperature: ");
			Serial.print(t);
			Serial.println(" *C");
			t=t*10;
			h=h*10;
			os_sprintf(temp, "%d.%01d",(int)(t/10),(int)t%10);
			os_sprintf(hum, "%d.%01d",(int)(h/10),(int)h%10 );

			if (sensorValue & 1)
				os_sprintf(temp2, "http://%s/update?key=%s&field6=%s&field2=%s&status=dev_ip:192.168.0.188",THINGSPEAK_SERVER,THINGSPEAK_API_KEY,temp,hum);
			else
			    os_sprintf(temp2, "http://%s/u?k&f6=%s&f2=%s&s",KEIL_SERVER,temp,hum);

			thingSpeak.downloadString(String(temp2),onDataSent);

		}



}
#endif


#ifdef DS_18
void sendData()
{
	char hum[16];
	char temp[256];


#ifdef KEIL_SERVER_EXIST
	sensorValue++;					// two or one server
#else
	sensorValue+=2;
#endif


	if (thingSpeak.isProcessing())
	{
      //  thingSpeak.reset();
        LastErr[sensorValue&1]=6;
		 Serial.println("LastErr=6");
		return; // We need to wait while request processing was completed
	}


	float h = 0; //dht.readHumidity();
		float t = Read_DS18();

		// check if returns are valid, if they are NaN (not a number) then something went wrong!
		if (isnan(t)  || (t == 5000))
		{
			Serial.println("Failed to read from DS1820 - not send");
		} else {
			 Serial.println(" ");
			if (sensorValue & 1)
			   Serial.println("THINGSPEAK");
			else
			   Serial.println("KEIL");

			Serial.print("Temperature: ");
			Serial.print(t);
			Serial.println(" *C");
			t=t*100;
			t=t/10;


#ifdef BMP_180
				Read_BMP180 ();
				if (sensorValue & 1)
					os_sprintf(temp, "http://%s/update?key=%s&field1=%d.%01d&field7=%d.%01d&field8=%d.%01d&status=dev_ip:192.168.0.180",THINGSPEAK_SERVER,THINGSPEAK_API_KEY,(int)(t/10),(int)t%10,(int)(cctemp/10),(int)cctemp%10,(int)(ccpres/10),(int)ccpres%10);
				else
				    os_sprintf(temp, "http://%s/u?k&f1=%d.%01d&f7=%d.%01d&f8=%d.%01d&k",KEIL_SERVER,(int)(t/10),(int)t%10,(int)(cctemp/10),(int)cctemp%10,(int)(ccpres/10),(int)ccpres%10);
#else
				if (sensorValue & 1)
					os_sprintf(temp, "http://%s/update?key=%s&field5=%d.%01d&status=dev_ip:192.168.0.186",THINGSPEAK_SERVER,THINGSPEAK_API_KEY,(int)(t/10),(int)t%10);
				else
				    os_sprintf(temp, "http://%s/u?k&f5=%d.%01d&k",KEIL_SERVER,(int)(t/10),(int)t%10);

#endif
		     thingSpeak.downloadString(String(temp),onDataSent);
		}
}
#endif


// Will be called when WiFi station was connected to AP
void connectOk()
{

	NtpSming = new ntpClientSming();

	Serial.println("I'm CONNECTED");
	Serial.print("My IP: ");
	Serial.println(WifiStation.getIP().toString());
	Serial.print("My gate: ");
	Serial.println(WifiStation.getNetworkGateway().toString());

	telnetServer.listen(23);
	commandHandler.registerCommand(CommandDelegate("info","Info from ESP via Telnet\r\n","testGroup", EspInfoCommand));
	commandHandler.registerCommand(CommandDelegate("reset","Software reset via Telnet\r\n","testGroup", ResetCommand));

	test_spiffs();


	// Start send data loop
	procTimer.initializeMs(60 * 1000, sendData).start(true); // every 60 seconds
	resetTimer.initializeMs(240 * 1000, resetData).start(true); // every 240 seconds if errors
}

// Will be called when WiFi station timeout was reached
void connectFail()
{
	Serial.println("I'm NOT CONNECTED. Need help :(");



	WifiStation.waitConnection(connectOk, 20, connectFail); // We recommend 20+ seconds for connection timeout at start

}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Disable debug output to serial

#ifdef DS_18
	ds.begin(); // It's required for one-wire initialization!
#endif
#ifdef DTH_22
	dht.begin();
#endif
	Serial.println("Start......\n\n\n");

#ifdef BMP_180
	Wire.pins(12,13);
	Wire.begin();

#endif
	SystemClock.setTimeZone(2);

	SystemClock.setTime(dateTime);

	Serial.print("Time is not set so far: ");
	Serial.println(SystemClock.getSystemTimeString());

	SdateTime=SystemClock.now();

	LastErr[0]=-1;
	LastErr[1]=-1;
	sensorValue=1; //thingSpeak

	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiStation.enable(true);
	WifiAccessPoint.enable(false);




	// Run our method when station was connected to AP (or not connected)
	WifiStation.waitConnection(connectOk, 20, connectFail); // We recommend 20+ seconds for connection timeout at start
}
