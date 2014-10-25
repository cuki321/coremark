//#define COREMARK_ARDUINO 1

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);         // Open serial over USB.
  while(!Serial.available())  // Wait here until the user presses ENTER 
    SPARK_WLAN_Loop();        // in the Serial Terminal. Call the BG Tasks
                              // while we are hanging around doing nothing.
}

void loop() {
  // put your main code here, to run repeatedly:
  int total_time = 0;
  total_time = coremark_main();
  Serial.println("Coremark Running");

}
