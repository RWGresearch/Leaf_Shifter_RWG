# GPIO Output Test - Hardware Verification

## 📋 **Purpose**

This simple test program verifies your **GPIO hardware** is working correctly by cycling through all gear positions.

Use this to confirm:
- ✅ TCA9534 GPIO expander is responding
- ✅ All GPIO patterns output correctly
- ✅ Shifter hardware responds to each gear
- ✅ No hardware wiring issues

---

## 🔄 **Test Sequence**

The program repeats this cycle forever:

```
START: HOME
  ↓
  Wait 3 seconds
  ↓
PARK (100ms pulse)
  ↓
HOME
  ↓
  Wait 3 seconds
  ↓
REVERSE (100ms pulse)
  ↓
HOME
  ↓
  Wait 3 seconds
  ↓
DRIVE (100ms pulse)
  ↓
HOME
  ↓
  Wait 3 seconds
  ↓
NEUTRAL (1100ms pulse)
  ↓
HOME
  ↓
  Cycle repeats...
```

---

## 📊 **Serial Output**

```
========================================
GPIO Output Test - Hardware Verification
========================================

TCA9534 initialized at 0x39
All pins configured as outputs

Starting test sequence...

========================================
TEST CYCLE START
========================================

→ Testing PARK
  GPIO OUTPUT: PARK = 0x45 → 0xBA (inverted) = 1011 1010
  Holding for 100 ms...
  Returning to HOME
  GPIO OUTPUT: HOME = 0x54 → 0xAB (inverted) = 1010 1011
  Waiting 3 seconds...

→ Testing REVERSE
  GPIO OUTPUT: REVERSE = 0x98 → 0x67 (inverted) = 0110 0111
  Holding for 100 ms...
  Returning to HOME
  GPIO OUTPUT: HOME = 0x54 → 0xAB (inverted) = 1010 1011
  Waiting 3 seconds...

→ Testing DRIVE
  GPIO OUTPUT: DRIVE = 0x32 → 0xCD (inverted) = 1100 1101
  Holding for 100 ms...
  Returning to HOME
  GPIO OUTPUT: HOME = 0x54 → 0xAB (inverted) = 1010 1011
  Waiting 3 seconds...

→ Testing NEUTRAL
  GPIO OUTPUT: NEUTRAL = 0x5A → 0xA5 (inverted) = 1010 0101
  Holding for 1100 ms...
  Returning to HOME
  GPIO OUTPUT: HOME = 0x54 → 0xAB (inverted) = 1010 1011
  Waiting 3 seconds...

========================================
TEST CYCLE COMPLETE - Restarting...
========================================
```

---

## 🔧 **How to Use**

### **1. Upload Test Program:**
- Open `test_output.ino` in Arduino IDE
- Board: ESP32C3 Dev Module
- Upload to your hardware

### **2. Open Serial Monitor:**
- Baud rate: 115200
- Watch for "TCA9534 initialized" message
- If you see "ERROR: TCA9534 not found" → check I2C wiring

### **3. Watch Your Shifter:**
Every 3 seconds, a new gear should activate:
- **Does PARK work?** → Hardware good for PARK
- **Does REVERSE work?** → Hardware good for REVERSE
- **Does DRIVE work?** → Hardware good for DRIVE
- **Does NEUTRAL work?** → Hardware good for NEUTRAL

### **4. Check Serial Output:**
- Verify GPIO values match what you expect
- All patterns should show as inverted (0xBA, 0x67, 0xCD, 0xA5, etc.)

---

## ✅ **What to Look For**

### **✓ GOOD - All Working:**
```
All gears trigger correctly
Shifter responds to each pattern
No I2C errors in serial
Clean output showing all patterns
```
→ **Hardware is GOOD! Problem is in main code logic.**

### **❌ BAD - Some Not Working:**
```
PARK works ✓
REVERSE doesn't work ✗
DRIVE works ✓
NEUTRAL doesn't work ✗
```
→ **Hardware issue! Check:**
- GPIO pin connections on 0x39
- Shifter actuator wiring
- GPIO pattern values might be wrong

### **❌ BAD - Nothing Works:**
```
ERROR: TCA9534 not found at 0x39!
```
→ **I2C issue! Check:**
- I2C wiring (SDA, SCL)
- I2C address (should be 0x39)
- I2C pull-up resistors
- Power to TCA9534

---

## 🎯 **Expected Results**

If hardware is good, you should see:
- ✅ PARK actuates correctly
- ✅ REVERSE actuates correctly
- ✅ DRIVE actuates correctly
- ✅ NEUTRAL actuates correctly (holds longer - 1.1s)
- ✅ HOME position between each

---

## 📝 **GPIO Patterns (for reference)**

| Gear | Pattern | Inverted | Binary (inverted) |
|------|---------|----------|-------------------|
| HOME | 0x54 | 0xAB | 1010 1011 |
| PARK | 0x45 | 0xBA | 1011 1010 |
| REVERSE | 0x98 | 0x67 | 0110 0111 |
| DRIVE | 0x32 | 0xCD | 1100 1101 |
| NEUTRAL | 0x5A | 0xA5 | 1010 0101 |

---

## 🔧 **Configuration**

To change settings, edit these in `test_output.ino`:

```cpp
#define DELAY_BETWEEN_GEARS     3000    // Time between gears (ms)
#define GPIO_PULSE_STANDARD     100     // Pulse time for most gears
#define GPIO_PULSE_NEUTRAL      1100    // Pulse time for NEUTRAL
#define INVERT_OUTPUT           true    // GPIO inversion
#define I2C_GPIO_ADDR           0x39    // TCA9534 address
```

---

## 🚀 **Next Steps**

### **If All Gears Work:**
→ Hardware is GOOD! Go back to main code and debug software logic.

### **If REVERSE/NEUTRAL Don't Work:**
→ Hardware problem! Check:
1. GPIO pin connections
2. Shifter actuator for those gears
3. Possibly wrong GPIO patterns for your hardware

### **If Nothing Works:**
→ I2C problem! Check wiring and address.

---

## 📞 **Troubleshooting**

**Serial shows "ERROR: TCA9534 not found":**
- Check I2C wiring (GPIO 8 = SDA, GPIO 9 = SCL)
- Verify address is 0x39
- Check 4.7kΩ pull-up resistors on SDA/SCL
- Verify 5V power to TCA9534

**Some gears work, others don't:**
- GPIO patterns might be wrong for your hardware
- Shifter actuator might be faulty
- Check connections to specific GPIO pins

**All gears work in test, but not in main code:**
- Problem is in main code logic, not hardware!
- Debug the threshold detection or state machine

---

## ✅ **Ready to Test!**

Upload this test program and watch your shifter cycle through all gears. This will immediately tell you if the problem is hardware or software!
