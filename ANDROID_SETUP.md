# Android Auto Setup Requirements

For Android Auto to work, your Android phone needs the following:

## Required Android Settings

1. **Enable Developer Options:**
   - Go to Settings > About phone
   - Tap "Build number" 7 times
   - Developer options will appear in Settings

2. **Enable USB Debugging:**
   - Go to Settings > System > Developer options
   - Enable "USB debugging"
   - This is required for AOAP (Android Open Accessory Protocol) mode

3. **Install Android Auto App:**
   - Install Android Auto from Google Play Store
   - Open Android Auto app and complete setup

4. **USB Connection Mode:**
   - When you connect via USB, select "File Transfer" or "MTP" mode
   - The phone should NOT be in "Charging only" mode

5. **Accept Accessory Mode Prompt:**
   - When the head unit tries to switch the phone to AOAP mode, Android will show a popup
   - You MUST accept this prompt on your phone
   - The prompt says something like "Allow access to USB accessory?"
   - Check "Always allow from this computer" if available

## WSL2 Specific Issues

WSL2 has limited USB support. Even with USB forwarding (usbipd), USB control transfers may not work correctly.

### Troubleshooting in WSL2:

1. **Check USB Forwarding:**
   ```bash
   # On Windows (PowerShell):
   usbipd wsl list
   usbipd wsl attach --busid <BUSID>
   ```

2. **Verify Device is Accessible:**
   ```bash
   # In WSL2:
   lsusb
   # Should show your device
   ```

3. **Check Permissions:**
   ```bash
   # In WSL2:
   groups
   # Should include 'plugdev'
   ```

4. **If Query Chain Hangs:**
   - The device might not be responding to USB control transfers
   - Try unplugging and replugging the phone
   - Check if Android shows any prompts
   - Try on native Linux instead of WSL2 for better USB support

## Alternative: Use Native Linux

For best results, consider running this on native Linux (not WSL2) where USB support is full-featured.

