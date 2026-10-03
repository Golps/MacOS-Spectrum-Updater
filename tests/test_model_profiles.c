#include "../src/known-firmware.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 const char *beta="d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3";
 const char *glossy="0b5d8350af9997e35b8591b4d5585ae859988129b72b4e7b8d55a3ab68ea56a9";
 const char *v108="136d982c65f8c7615c6f5dd4adc6f9bfa8226f1f877e6e6dcff3d7bb28591757";
 const char *pd="62879652a96b8098cb240c4a4290828304afc6709b3948066ccb0d1de49e0fe1";
 assert(sp_known_model(beta,"ES07D03"));assert(sp_known_model(beta,"ES07DC9"));assert(sp_known_model(beta,"ES07E30"));
 assert(sp_known_model(glossy,"ES07DC9"));assert(!sp_known_model(glossy,"ES07D03"));assert(!sp_known_model(glossy,"ES07E30"));
 assert(!sp_known_model(v108,"ES07DC9"));assert(sp_known_model(v108,"ES07E30"));
 assert(!sp_known_model(beta,"ES07D02"));assert(!sp_known_model(beta,"ES07E91"));
 assert(sp_label_confirmation_required(beta,glossy));
 assert(!sp_label_confirmation_required(beta,beta));
 assert(!sp_label_confirmation_required(v108,v108));
 assert(!sp_label_confirmation_required(NULL,glossy));
 assert(!sp_known_model(pd,"ES07D03"));assert(sp_supported_model("ES07D02"));assert(!sp_supported_model(NULL));assert(!sp_known_model(NULL,"ES07D03"));
 assert(sp_known_model("5f0a7bf92119fcc5965f09985d41645a062e86fabd993de86635a515b6e92162","ES07D02"));
 assert(!sp_known_model("5f0a7bf92119fcc5965f09985d41645a062e86fabd993de86635a515b6e92162","ES07D03"));
 puts("20 model authorization checks passed; shared-image label guards and incompatible targets rejected.");
}
