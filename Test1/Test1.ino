void setup() {
  Serial.begin(115200);
  pinMode(21, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  pinMode(8, OUTPUT);
  pinMode(6, OUTPUT);
  delay(1000);
  digitalWrite(8, HIGH);
  digitalWrite(6, HIGH);  
}

void loop() {
  Serial.printf("%d %d\n", digitalRead(21), digitalRead(10));
  if (!digitalRead(21)) {
    digitalWrite(8, HIGH);
  } else {
    digitalWrite(8, LOW);
  }
  if (!digitalRead(10)) {
    digitalWrite(6, HIGH);
  } else {
    digitalWrite(6, LOW);
  }
  delay(100);
}
