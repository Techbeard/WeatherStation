// WiFi Config
const char ssid[] = "Undercover2.0";  //  your network SSID (name)
const char pass[] = "1998_derHund_98";       // your network password

// Database Config
const char* host      = "192.168.178.84";
const int influxPort  = 8086;
const String url      = "/write?db=Messdaten";
const String auth     = "VmFsZToxOTk4X2Rlckh1bmRfOTg=";
const String measurement = "Wohnung"; //(measurement = like database table)
const String sensorName  = "Esszimmer";
const String outTempQueryUrl = "/query?q=select%20last(temperature)%20from%20Wohnung%20where%20sensorName%3D%27outdoor%27%20&db=Messdaten";

//Time Config
// const int timeZone = 1;  //Germany Winterzeit
const int timeZone = 2;  //Germany Sommerzeit
