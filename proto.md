# Client to Server

## 0x00 Hello

| Off   | Type     | Value    |
|-------|----------|----------|
| 0x0   | u8       | ver      |
| 0x1   | u8       | len      |
| 0x2   | u8[len]  | name     |

## 0x01 Proc
| Off   | Type     | Value    |
|-------|----------|----------|
| 0x0   | u8       | flags    |
| 0x1   | u32      | pid      |

## 0x02 Peek
| Off   | Type     | Value    |
|-------|----------|----------|
| 0x0   | u8       | flags    |
| 0x1   | u32      | src      |
| 0x5   | u32      | len      |

## 0x03 Poke
| Off   | Type      | Value    |
|-------|-----------|----------|
| 0x0   | u8        | flags    |
| 0x1   | u32       | dst      |
| 0x5   | u32       | len      |
| 0x9   | u8[len]   | data     |

# Server to Client

## 0x00 Hello

| Off   | Type     | Value    |
|-------|----------|----------|
| 0x0   | u8       | success  |

## 0x01 Proc
| Off   | Type     | Value    |
|-------|----------|----------|
| 0x0   | u8       | success  |

## 0x02 Peek
| Off   | Type     | Value    |
|-------|----------|----------|
| 0x0   | u8       | success  |
| 0x1   | u32      | len      |
| 0x5   | u8[len]  | data     |

or if success == 0:

| Off   | Type     | Value    |
|-------|----------|----------|
| 0x0   | u8       | success  |

## 0x03 Poke
| Off   | Type      | Value    |
|-------|-----------|----------|
| 0x0   | u8        | success  |