#ifndef SP_KNOWN_FIRMWARE_H
#define SP_KNOWN_FIRMWARE_H
#include <stdbool.h>
#include <string.h>

enum {
    SP_MODEL_D03 = 1u << 0,
    SP_MODEL_DC9 = 1u << 1,
    SP_MODEL_E30 = 1u << 2,
    SP_MODEL_D02 = 1u << 3
};

typedef struct {
    const char *sha256;
    unsigned models;
    bool installable;
} sp_firmware_profile;

/* Keep the scaler authorization table compact and data-driven. The public
   compatibility manifest is checked against this table in the offline suite. */
static inline const sp_firmware_profile *sp_profile_for_hash(const char *sha) {
    static const sp_firmware_profile profiles[] = {
        {"536d94f761d84fa7d8b616dbf5442b81a2d470f2cb7112cba213bc4fbd10add9", SP_MODEL_D03, true},
        {"199bb51c3d31023ffb37542acc250af4bd66cc7059f9a221697ce9c6c5b27ab1", SP_MODEL_D03, true},
        {"136d982c65f8c7615c6f5dd4adc6f9bfa8226f1f877e6e6dcff3d7bb28591757", SP_MODEL_D03 | SP_MODEL_E30, true},
        {"d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3", SP_MODEL_D03 | SP_MODEL_DC9 | SP_MODEL_E30, true},
        {"0b5d8350af9997e35b8591b4d5585ae859988129b72b4e7b8d55a3ab68ea56a9", SP_MODEL_DC9, true},
        {"5f0a7bf92119fcc5965f09985d41645a062e86fabd993de86635a515b6e92162", SP_MODEL_D02, true},
        {"e319da9490df8cec4ff94677385e57a560899a9ef335b6dc7e9b052bf3bcc9f0", SP_MODEL_D02, true},
        /* Historical development images are recognized only as installed D03
           images so a user can migrate back to stock. */
        {"6f0548944344c9ef8e16a6d40eabef4872f9bfc4dab7a918d83899058e2b14bb", SP_MODEL_D03, false},
        {"461993270a3a4458e38f5d10d157ce5b091bd5db2beb6baec78b708aaa61dd7a", SP_MODEL_D03, false}
    };
    if (!sha) return NULL;
    for (unsigned i = 0; i < sizeof profiles / sizeof profiles[0]; ++i)
        if (!strcmp(sha, profiles[i].sha256)) return &profiles[i];
    return NULL;
}

static inline unsigned sp_model_bit(const char *model) {
    if (!model) return 0;
    if (!strcmp(model, "ES07D03")) return SP_MODEL_D03;
    if (!strcmp(model, "ES07DC9")) return SP_MODEL_DC9;
    if (!strcmp(model, "ES07E30")) return SP_MODEL_E30;
    if (!strcmp(model, "ES07D02")) return SP_MODEL_D02;
    return 0;
}

static inline unsigned sp_model_mask_for_hash(const char *sha) {
    const sp_firmware_profile *profile = sp_profile_for_hash(sha);
    return profile ? profile->models : 0;
}

static inline bool sp_known_es07d03(const char *sha) {
    return (sp_model_mask_for_hash(sha) & SP_MODEL_D03) != 0;
}

/* Model authorization is independent of USB product names. Shared scaler
   images establish a set of possible models, not a unique physical label. */
static inline bool sp_known_model(const char *sha, const char *model) {
    unsigned bit = sp_model_bit(model);
    return bit != 0 && (sp_model_mask_for_hash(sha) & bit) != 0;
}

static inline bool sp_supported_model(const char *model) {
    return sp_model_bit(model) != 0;
}

/* If the installed scaler image is shared by several physical model profiles,
   a target that is not valid for every one of those possibilities needs an
   independent physical-label confirmation before a write. */
static inline bool sp_label_confirmation_required(const char *installed_sha, const char *target_sha) {
    unsigned installed = sp_model_mask_for_hash(installed_sha);
    unsigned target = sp_model_mask_for_hash(target_sha);
    bool shared = installed && (installed & (installed - 1u));
    return shared && target && (target & installed) != installed;
}

#endif
