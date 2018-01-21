#define MOSI 53 // ATF22V10 pin 11
#define MISO 52 // ATF22V10 pin 14
#define SCK 50  // ATF22V10 pin 10
#define PREN 51 // 12V switch for ATF22V10 pin 2
#define nSTR 49 // ATF22V10 pin 13
#define OLMC 48 // ATF22V10 pin 8
#define WRITE 46 // ATF22V10 pin 3
#define ERASE 47 // ATF22V10 pins 4, 6, 7, 9

void enable_programming() {
  digitalWrite(nSTR, HIGH);
  delayMicroseconds(20);
  digitalWrite(PREN, HIGH);
  delay(50);  
}

void disable_programming() {
  digitalWrite(PREN, LOW);  
  delayMicroseconds(20);
}

// For reading
void short_pulse_strobe() {
  digitalWrite(nSTR, LOW);
  delayMicroseconds(1);
  digitalWrite(nSTR, HIGH);  
}

// For writing
void long_pulse_strobe() {
  digitalWrite(nSTR, LOW);
  delay(20);
  digitalWrite(nSTR, HIGH);  
}

// For erasing
void very_long_pulse_strobe() {
  digitalWrite(nSTR, LOW);
  delay(200);
  digitalWrite(nSTR, HIGH);  
}

// Data is written msb first.
void _write_data(uint8_t* buffer, uint8_t bits, bool end) {
  uint8_t* ptr = buffer;
  uint8_t bytes = (bits+7)/8;
  uint8_t nbits = 0;
  for (int i = 0; i < bytes; i++, ptr++) {
    for (int j = 7; j >= 0; j--, nbits++) {
      if (nbits >= bits) return;
      int c = (*ptr & (1 << j)) ? HIGH : LOW;
      digitalWrite(MOSI, c);
      // The last bit gets clocked in not by SCK, but by nSTR,
      // so we leave MOSI set for the next strobe.
      if (nbits == bits - 1 && end) return;
      digitalWrite(SCK, HIGH);
      delayMicroseconds(1);
      digitalWrite(SCK, LOW);
      delayMicroseconds(1);
    }
  }  
}

void write_data(uint8_t* buffer, uint8_t bits) {
  _write_data(buffer, bits, false);
}

void write_data_end(uint8_t* buffer, uint8_t bits) {
  _write_data(buffer, bits, true);
}

void send_column(uint8_t column) {
  column <<= 2;
  write_data_end(&column, 6);
}

void set_column(uint8_t column) {
  send_column(column);
  short_pulse_strobe();
  digitalWrite(MOSI, LOW);
  delayMicroseconds(1);  
}

// Data is read msb first
void read_data(uint8_t* buffer, uint8_t bits) {
  uint8_t* ptr = buffer;
  uint8_t bytes = (bits+7)/8;
  uint8_t nbits = 0;
  for (int i = 0; i < bytes; i++, ptr++) {
    *ptr = 0;
    for (int j = 0; j < 8; j++, nbits++) {
      *ptr <<= 1;
      if (nbits >= bits) continue; // zero-fills the rest
      int c = digitalRead(MISO);
      if (c == HIGH) *ptr |= 1;
      // Don't have to clock after the last bit
      if (nbits != bits - 1) {
        digitalWrite(SCK, HIGH);
        delayMicroseconds(1);
        digitalWrite(SCK, LOW);
        delayMicroseconds(1);
      }
    }
  }
}

// Buffer must be at least 17 bytes
void read_column(uint8_t* buffer, uint8_t column) {
  set_column(column);
  read_data(buffer, 132);
}

// Buffer must be at least 17 bytes
void write_column(uint8_t* buffer, uint8_t column) {
  write_data(buffer, 132);
  send_column(column); // yes, the column comes last
  digitalWrite(WRITE, HIGH);
  long_pulse_strobe();
  digitalWrite(WRITE, LOW);
}

// Buffer must be at least 9 bytes
void read_id(uint8_t* buffer) {
  set_column(0x3A); // 0x1D, 0
  read_data(buffer, 72);  
}

// Buffer must be at least 3 bytes
void read_olmc(uint8_t* buffer) {
  digitalWrite(OLMC, HIGH);
  short_pulse_strobe();
  read_data(buffer, 20);
  digitalWrite(OLMC, LOW);
}

// Buffer must be at least 3 bytes
void write_olmc(uint8_t* buffer) {
  digitalWrite(OLMC, HIGH);
  write_data_end(buffer, 20);  
  digitalWrite(WRITE, HIGH);
  long_pulse_strobe();
  digitalWrite(WRITE, LOW);
  digitalWrite(OLMC, LOW);
}

// powerdown bit not working yet

// Buffer must be at least 1 byte
void read_powerdown(uint8_t* buffer) {
  set_column(0x3B);
  read_data(buffer, 1);
}

void write_powerdown(uint8_t* buffer) {
  write_data(buffer, 1);
  send_column(0x3B);  // yes, the column comes last
  digitalWrite(WRITE, HIGH);
  long_pulse_strobe();
  digitalWrite(WRITE, LOW);
}

void erase() {
  digitalWrite(WRITE, HIGH);
  digitalWrite(ERASE, HIGH);
  digitalWrite(OLMC, HIGH);
  very_long_pulse_strobe();
  digitalWrite(OLMC, LOW);
  digitalWrite(ERASE, LOW);
  digitalWrite(WRITE, LOW);
}

void serial_out_data(uint8_t* buffer, uint8_t bytes) {
  for (int i = 0; i < bytes; i++) {
    char tmp[3];
    sprintf(tmp, "%02X", buffer[i]);
    Serial.print(tmp);
    Serial.print(" ");
  }
  Serial.print("\n");  
}

void setup_pins() {
  digitalWrite(PREN, LOW);
  digitalWrite(MOSI, LOW);
  digitalWrite(SCK, LOW);
  digitalWrite(nSTR, HIGH);
  digitalWrite(OLMC, LOW);
  digitalWrite(ERASE, LOW);
  digitalWrite(WRITE, LOW);
  pinMode(PREN, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK, OUTPUT);
  pinMode(nSTR, OUTPUT);
  pinMode(OLMC, OUTPUT);
  pinMode(WRITE, OUTPUT);
  pinMode(ERASE, OUTPUT);  
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  setup_pins();
  
  enable_programming();

  erase();

  uint8_t wdata[17] = {
    0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0xDE, 0x00,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x55, 0x66, 0x77,
    0x88
  };
  write_column(wdata, 0x2C);
  uint8_t wolmc_data[3] = {0xF1, 0x35, 0x79};
  write_olmc(wolmc_data);

  // powerdown bit not working yet
  //uint8_t wpowerdown_data[1] = {0x00};
  //write_powerdown(wpowerdown_data);
  
  uint8_t id[9];
  uint8_t olmc[3];
  uint8_t data[17];

  // read 72 bit ID
  read_id(id);
  serial_out_data(id, sizeof(id));

  // read 132 bits per column. The last column is
  // the user data column.
  for (uint8_t column = 0; column < 0x2D; column++) {
    read_column(data, column);
    serial_out_data(data, sizeof(data));
  }

  // read 20 bits of OLMC settings
  read_olmc(olmc);
  serial_out_data(olmc, sizeof(olmc));

  // read powerdown bit (not working yet)
  uint8_t powerdown[1];
  read_powerdown(powerdown);
  serial_out_data(powerdown, sizeof(powerdown));

  disable_programming();
}

void loop() {
}
