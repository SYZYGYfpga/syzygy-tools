szg_i2cwrite 1 0x30 0x9000 0x4b
szg_i2cwrite 1 0x30 0x9003 0x00
szg_i2cwrite 1 0x30 0x9006 0x01

szg_i2cwrite 1 0x32 0x9000 0x4b
szg_i2cwrite 1 0x32 0x9003 0x00
szg_i2cwrite 1 0x32 0x9006 0x01

szg_i2cwrite 1 0x33 0x9000 0x4b
szg_i2cwrite 1 0x33 0x9003 0x00
szg_i2cwrite 1 0x33 0x9006 0x01

szg_i2cread 1 0x30 0x9000
szg_i2cread 1 0x30 0x9003
szg_i2cread 1 0x30 0x9006

szg_i2cread 1 0x32 0x9000
szg_i2cread 1 0x32 0x9003
szg_i2cread 1 0x32 0x9006

szg_i2cread 1 0x33 0x9000
szg_i2cread 1 0x33 0x9003
szg_i2cread 1 0x33 0x9006

