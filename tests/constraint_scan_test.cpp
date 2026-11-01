#include "ConstraintScan.h"
#include <assert.h>
#include <stdio.h>
#include <random>
#include <algorithm>
#include <vector>
#include <unordered_map>

static coc::Planner planner;
static const uint8_t font[10][7] = {
 {1,1,1,1,1,1,0},{0,1,1,0,0,0,0},{1,1,0,1,1,0,1},
 {1,1,1,1,0,0,1},{0,1,1,0,0,1,1},{1,0,1,1,0,1,1},
 {0,0,1,1,1,1,1},{1,1,1,0,0,0,0},{1,1,1,1,1,1,1},{1,1,1,0,0,1,1}
};
static uint32_t pattern(int h, int m) {
 int digits[4] = {h < 10 ? -1 : h/10, h%10, m/10, m%10};
 uint32_t target = 0;
 for (int d=0;d<4;++d) if (digits[d]>=0)
   for (int s=0;s<7;++s) if (font[digits[d]][s]) target |= 1u<<(d*7+s);
 return target;
}
// Independent oracle uses integer pin states, not planner's selection/validation.
static std::vector<uint32_t> oraclePatterns(uint32_t target, const coc::Limits& limits) {
 std::vector<uint32_t> patterns;
 for(int code=0;code<729;++code) {
   int n=code, states[6];
   for(int p=0;p<6;++p,n/=3) states[p]=n%3;
   uint32_t selected=0;
   unsigned src[6]{}, sink[6]{}, count=0;
   bool allowed=true;
   for(unsigned i=0;i<28;++i) {
     auto pair=coc::kMap[i];
     if(states[pair.hi]!=1 || states[pair.lo]!=2) continue;
     selected |= 1u<<i;
     ++count;
     if(++src[pair.hi]>limits.perSource || ++sink[pair.lo]>limits.perSink) allowed=false;
   }
   if(allowed && selected && !(selected & ~target) && count<=limits.total)
     patterns.push_back(selected);
 }
 return patterns;
}
static int minimum(uint32_t left,const std::vector<uint32_t>& patterns,
                   std::unordered_map<uint32_t,int>& memo) {
 if(!left) return 0;
 auto it=memo.find(left); if(it!=memo.end()) return it->second;
 int best=28;
 uint32_t first=left & (~left+1u);
 for(auto p:patterns) if((p&first) && !(p&~left))
   best=std::min(best,1+minimum(left^p,patterns,memo));
 return memo[left]=best;
}
static void independentCheck(const coc::Plan& plan,uint32_t target,const coc::Limits& limits) {
 assert(coc::verify(plan,target,limits));
 auto patterns=oraclePatterns(target,limits);
 uint32_t covered=0;
 for(unsigned s=0;s<plan.count;++s) {
   auto slot=plan.slots[s];
   assert(std::find(patterns.begin(),patterns.end(),slot.segments)!=patterns.end());
   assert(!(covered & slot.segments)); covered |= slot.segments;
 }
 assert(covered==target);
}
int main() {
 coc::Limits safe;
 coc::Limits paired{2,2,2,4096};
 coc::Limits broad{9,5,5,100000};
 for(auto limits:{safe,paired,broad}) {
   // Exhaust every subset of the six installed edges among the first three pins.
   uint32_t edges[6]; unsigned edgeCount=0;
   for(unsigned i=0;i<28;++i)
     if(coc::kMap[i].hi<3 && coc::kMap[i].lo<3) edges[edgeCount++]=1u<<i;
   assert(edgeCount==6);
   for(unsigned subset=0;subset<64;++subset) {
     uint32_t target=0;
     for(unsigned b=0;b<6;++b) if(subset&(1u<<b)) target |= edges[b];
     auto plan=planner.build(target,limits);
     independentCheck(plan,target,limits);
     std::unordered_map<uint32_t,int> memo;
     assert(plan.optimal);
     assert(plan.count==minimum(target,oraclePatterns(target,limits),memo));
   }
 }
 unsigned verified=0;
 for(int h=0;h<24;++h) for(int m=0;m<60;++m) {
   auto target=pattern(h,m);
   for(auto limits:{safe,paired}) {
     auto plan=planner.build(target,limits);
     independentCheck(plan,target,limits);
     assert(plan.nodes<=limits.searchNodes);
     if(limits.total==1) assert(plan.count==coc::bitCount(target));
     ++verified;
   }
 }
 std::mt19937 rng(20261031);
 for(unsigned i=0;i<1000;++i) {
   auto limits=paired;
   limits.total=1+rng()%9;
   limits.perSource=1+rng()%5;
   limits.perSink=1+rng()%5;
   limits.searchNodes=rng()%50;
   auto target=rng() & coc::kAllSegments;
   auto plan=planner.build(target,limits);
   independentCheck(plan,target,limits);
   assert(plan.nodes<=limits.searchNodes);
 }
 for(auto target:{pattern(12,34),pattern(11,11),pattern(23,59),coc::kAllSegments}) {
   auto plan=planner.build(target,broad);
   independentCheck(plan,target,broad);
   printf("lit=%u unrestricted digital groups=%u optimal=%d nodes=%u\n",
          plan.lit,plan.count,plan.optimal,plan.nodes);
 }
 auto broken=planner.build(pattern(12,34),paired);
 broken.slots[0].low |= broken.slots[0].high;
 assert(!coc::verify(broken,pattern(12,34),paired));
 coc::Limits noSearch{2,2,2,0};
 independentCheck(planner.build(coc::kAllSegments,noSearch),coc::kAllSegments,noSearch);
 printf("PASS: 192 oracle-optimal subsets, %u clock plans, 1000 random limits/patterns, corruption and zero-budget checks\n",verified);
}
