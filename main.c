#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ch34x/ch347_lib.h>
#include <sys/stat.h>
#include <unistd.h>

enum operations {SCAN, READ, WRITE};

#define I2C_READ_BUFFER_LENGTH 65536
#define I2C_WRITE_BUFFER_LENGTH 512

static unsigned char i2c_read_buffer[I2C_READ_BUFFER_LENGTH];
static unsigned char i2c_write_buffer[I2C_WRITE_BUFFER_LENGTH];

static void print_hex_buffer(unsigned char *buffer, int length)
{
  printf("   ");
  for (int i = 0; i < 16; i++)
    printf(" %02x", i);

  for (int i = 0; i < length; i++)
  {
    int row_no = i / 16;
    if (i % 16 == 0)
      printf("\n%02x:", row_no);
    printf(" %02x", *buffer++);
  }
  puts("");
}

static bool i2c_check(int fd, unsigned char address)
{
  return CH347StreamI2C(fd, 1, &address, 0, NULL);
}

static void i2c_scan(int fd)
{
  puts("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
  printf("00:   ");

  for (unsigned char i = 1; i <= 0x7F; i++)
  {
    if (i % 16 == 0)
      printf("\n%.2x:", i);
    bool rc = i2c_check(fd, i << 1);
    if (rc)
      printf(" %.2x", i);
    else
      printf(" --");
  }
  puts("");
}

static void usage(void)
{
  puts("Usage: CH347Eeprom <device> <speed>\n  scan\n  read i2c_address address_length address length");
  puts("  write i2c_address address_length address page_size file_name");
}

static void i2c_read(int fd, unsigned char i2c_address, int address_length, int address, int length)
{
  i2c_write_buffer[0] = i2c_address << 1;
  memcpy(i2c_write_buffer + 1, &address, address_length);
  bool rc = CH347StreamI2C(fd, address_length + 1, i2c_write_buffer, length, i2c_read_buffer);
  if (!rc)
  {
    puts("Failed to read I2C data.");
    return;
  }
  print_hex_buffer(i2c_read_buffer, length);
}

static void i2c_write(int fd, unsigned char i2c_address, int address_length, int address, int page_size, int length)
{
  unsigned char *p = i2c_read_buffer;
  while (length)
  {
    int l = length > page_size ? page_size : length;
    i2c_write_buffer[0] = i2c_address << 1;
    memcpy(i2c_write_buffer + 1, &address, address_length);
    memcpy(i2c_write_buffer + 1 + address_length, p, l);
    bool rc = CH347StreamI2C(fd, address_length + 1 + l, i2c_write_buffer, 0, NULL);
    if (!rc)
    {
      printf("Failed to write I2C data at address %x\n", address);
      return;
    }
    length -= l;
    p += l;
    address += l;
    usleep(5000);
  }
  puts("Done.");
}

int main(int argc, char **argv)
{
  if (argc < 4)
  {
    usage();
    return 1;
  }

  int mode;
  int speed = atoi(argv[2]);
  switch (speed)
  {
    case 20: mode = 0; break;
    case 100: mode = 1; break;
    case 400: mode = 2; break;
    case 750: mode = 3; break;
    case 50: mode = 4; break;
    case 200: mode = 5; break;
    case 1000: mode = 6; break;
    default:
      puts("Invalid speed.");
      return 1;
  }
  enum operations operation;
  if (!strcmp(argv[3], "scan"))
    operation = SCAN;
  else if (!strcmp(argv[3], "read"))
    operation = READ;
  else if (!strcmp(argv[3], "write"))
    operation = WRITE;
  else
  {
    puts("Invalid operation.");
    return 1;
  }

  if ((operation == READ && argc != 8) || (operation == WRITE && argc != 9))
  {
    usage();
    return 1;
  }
  unsigned long int i2c_address = 0;
  int address_length = 0;
  int address = 0;
  int length = 0;
  int page_size = 0;
  if (operation == READ || operation == WRITE)
  {
    i2c_address = strtoul(argv[4], NULL, 16);
    if (i2c_address > 0x7F)
    {
      puts("Invalid I2C address.");
      return 1;
    }
    address_length = atoi(argv[5]);
    if (address_length < 1 || address_length > 2)
    {
      puts("Invalid address length.");
      return 1;
    }
    address = atoi(argv[6]);
    if (address < 0 || address > (1<<8*address_length))
    {
      puts("Invalid address.");
      return 1;
    }
    length = atoi(argv[7]);
    if (length < 1 || length > (operation == READ ? I2C_READ_BUFFER_LENGTH : I2C_WRITE_BUFFER_LENGTH - 1 - address_length))
    {
      puts("Invalid length.");
      return 1;
    }
    if (operation == WRITE)
    {
      struct stat st;
      if (stat(argv[8], &st))
      {
        puts("Failed to stat file.");
        return 1;
      }
      if (st.st_size >= I2C_READ_BUFFER_LENGTH)
      {
        puts("File is too large.");
        return 1;
      }
      FILE *fd = fopen(argv[8], "rb");
      if (!fd)
      {
        puts("Failed to open file.");
        return 1;
      }
      fread(i2c_read_buffer, 1, st.st_size, fd);
      fclose(fd);
      page_size = length;
      length = (int)st.st_size;
    }
  }

  int fd = CH347OpenDevice(argv[1]);
  if (fd < 0) {
    puts("CH347OpenDevice failed.");
    return 2;
  }
  printf("Open device %s succeed, fd: %d\n", argv[1], fd);

  bool ret = CH347I2C_Set(fd, mode);
  if (!ret)
  {
    puts("Failed to init I2C interface.");
    return 3;
  }
  puts("CH347 I2C interface init succeed.");

  switch (operation)
  {
    case SCAN:
      i2c_scan(fd);
      break;
    case READ:
      i2c_read(fd, (unsigned char)i2c_address, address_length, address, length);
      break;
    case WRITE:
      i2c_write(fd, (unsigned char)i2c_address, address_length, address, page_size, length);
      break;
  }

  /* close the device */
  if (CH347CloseDevice(fd))
    puts("Close device succeed.");
  else
    puts("Close device failed.");
  return 0;
}