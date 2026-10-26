#pragma once
void AttachClassicDevice(void* device,unsigned type);
void ClassicKeyboardState(const unsigned char* keys);
void ClassicMouseState(long x,long y,long wheel,const unsigned char* buttons,unsigned count);
void ClassicBufferedInput(unsigned type,unsigned offset,unsigned value);
void RequestClassicNativeTestKey(unsigned scan,unsigned holdMs);
void ClassicInputDiagnostic(const char* message);
bool ClassicNativeTestActive(unsigned scan);
