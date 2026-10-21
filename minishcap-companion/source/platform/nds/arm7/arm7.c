#include <nds.h>
void VblankHandler(void) {}
void VcountHandler(void) { inputGetAndSend(); }
int main(void) {
    readUserSettings();
    irqInit();
    fifoInit();
    initClockIRQ();
    SetYtrigger(80);
    installSoundFIFO();
    installSystemFIFO();
    irqSet(IRQ_VCOUNT, VcountHandler);
    irqSet(IRQ_VBLANK, VblankHandler);
    irqEnable(IRQ_VBLANK | IRQ_VCOUNT);
    while (1) swiWaitForVBlank();
}
