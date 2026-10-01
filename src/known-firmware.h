#ifndef SP_KNOWN_FIRMWARE_H
#define SP_KNOWN_FIRMWARE_H
#include <stdbool.h>
#include <string.h>
/* Exact validated ES07D03 images; a VIA bridge or scaler-family signature alone
   is insufficient model identification for automatic installation. */
static inline bool sp_known_es07d03(const char *sha) {
    static const char *const hashes[] = {
        "536d94f761d84fa7d8b616dbf5442b81a2d470f2cb7112cba213bc4fbd10add9",
        "199bb51c3d31023ffb37542acc250af4bd66cc7059f9a221697ce9c6c5b27ab1",
        "136d982c65f8c7615c6f5dd4adc6f9bfa8226f1f877e6e6dcff3d7bb28591757",
        "d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3",
        "6f0548944344c9ef8e16a6d40eabef4872f9bfc4dab7a918d83899058e2b14bb",
        "461993270a3a4458e38f5d10d157ce5b091bd5db2beb6baec78b708aaa61dd7a"
    };
    if (!sha) return false;
    for (unsigned i=0; i<sizeof hashes/sizeof hashes[0]; ++i)
        if (!strcmp(sha, hashes[i])) return true;
    return false;
}
/* Model authorization is independent of USB product names. Shared Beta03 is
   documented by Dough for the three 4K IPS variants. */
static inline bool sp_known_model(const char *sha,const char *model) {
    if(!sha||!model)return false;
    bool beta=!strcmp(sha,"d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3");
    bool dc9=!strcmp(sha,"0b5d8350af9997e35b8591b4d5585ae859988129b72b4e7b8d55a3ab68ea56a9");
    bool v108=!strcmp(sha,"136d982c65f8c7615c6f5dd4adc6f9bfa8226f1f877e6e6dcff3d7bb28591757");
    if(!strcmp(model,"ES07D03"))return sp_known_es07d03(sha);
    if(!strcmp(model,"ES07DC9"))return beta||dc9;
    if(!strcmp(model,"ES07E30"))return beta||v108;
    if(!strcmp(model,"ES07D02"))return !strcmp(sha,"5f0a7bf92119fcc5965f09985d41645a062e86fabd993de86635a515b6e92162")||!strcmp(sha,"e319da9490df8cec4ff94677385e57a560899a9ef335b6dc7e9b052bf3bcc9f0");
    return false;
}
static inline bool sp_supported_model(const char *model) {
    return model&&(!strcmp(model,"ES07D03")||!strcmp(model,"ES07DC9")||!strcmp(model,"ES07E30")||!strcmp(model,"ES07D02"));
}
#endif
