#include "CharlieDisplay.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
int pinMode[32]{}, pinLevel[32]{};
extern void onTimer();
static uint32_t currentlySelected() {
 uint32_t mask=0;
 for(unsigned i=0;i<coc::kSegments;++i) {
   auto pair=coc::kMap[i];
   auto hi=coc::kGpio[pair.hi], lo=coc::kGpio[pair.lo];
   if(pinMode[hi]==GPIO_MODE_OUTPUT && pinLevel[hi] &&
      pinMode[lo]==GPIO_MODE_OUTPUT && !pinLevel[lo]) mask|=1u<<i;
 }
 return mask;
}
static uint32_t oneExposureCycle() {
 uint32_t covered=0;
 for(unsigned q=0;q<28;++q) {
   onTimer(); auto selected=currentlySelected();
   assert(!(covered&selected)); covered|=selected;
   assert(coc::bitCount(selected)<=(COC_ENABLE_GROUPING?COC_MAX_SIMULTANEOUS_LEDS:1));
 }
 return covered;
}
int main() {
 pinMode[1]=GPIO_MODE_OUTPUT; pinLevel[1]=1;
 display.begin();
 assert(pinMode[1]==GPIO_MODE_INPUT);
 assert(pinMode[2]==GPIO_MODE_INPUT);
 display.setBrightness(255);
 display.print("1234");
 const uint32_t target=oneExposureCycle();
 assert(coc::bitCount(target)==16);
 auto stats=display.getScanStats();
 assert(stats.lit==16 && !stats.fallback);
 assert(stats.groups==(COC_ENABLE_GROUPING?8:16));
 // Every brightness value must give each target exactly gamma[level] pulses
 // per 64*28 timer calls, regardless of the number of simultaneous groups.
 display.setColon(true);
 for(unsigned level=0;level<256;++level) {
   display.setBrightness(level); display.setColonBrightness(level);
   unsigned histogram[28]{}, colonPulses=0;
   for(unsigned tick=0;tick<64*28;++tick) {
     onTimer(); auto actual=currentlySelected();
     assert(!(actual&~target));
     for(unsigned i=0;i<28;++i) if(actual&(1u<<i)) ++histogram[i];
     if(pinLevel[COLON_PIN]) ++colonPulses;
   }
   unsigned reference=histogram[1];
   for(unsigned i=0;i<28;++i)
     assert(histogram[i]==((target&(1u<<i))?reference:0));
   assert(colonPulses==reference);
   if(level==0) assert(reference==0);
   if(level==255) assert(reference==64);
 }
 // Null, blank, custom glyph and rapid pattern changes remain bounded.
 for(auto text:{"1111","8888","SYNC","APON","----","$C","", "9"}) {
   display.print(text);
   oneExposureCycle();
 }
 display.print(nullptr);
 assert(oneExposureCycle()==0);
 display.setColon(false); onTimer(); assert(!pinLevel[COLON_PIN]);
 struct tm t{}; t.tm_hour=12; t.tm_min=34;
 display.update(&t);
 assert(oneExposureCycle()==target);
 display.update(nullptr);
 assert(oneExposureCycle()==target);
 printf("PASS: actual display driver, grouping=%d; 256 brightness levels, equal exposure/colon, GPIO initialization and pattern changes\n",COC_ENABLE_GROUPING);
}
